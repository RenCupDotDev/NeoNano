import curses
import time

import config as cfgmod
from buffer import Buffer
from keys import build_keymap, read_key
from theme import build_theme, list_themes

CYCLE = ["left", "right", "none"]


class Editor:
    def __init__(self, stdscr, buf, cfg, theme, config_path):
        self.stdscr = stdscr
        self.buf = buf
        self.cfg = cfg
        self.theme = theme
        self.config_path = config_path
        self.keymap = {}
        self.idle_ms = -1
        self.top = 0
        self.left = 0
        self.msg = ""
        self.msg_error = False
        self.quit_armed = False
        self.should_quit = False
        self.undo_stack = []
        self.redo_stack = []
        self._group = None
        self.clipboard = ""
        self.last_needle = ""
        self.hooks = {}
        self.last_change = time.monotonic()
        self.plugins = []
        self.plugin_errors = []

    # ----- settings -----

    @property
    def tab(self):
        return int(self.cfg["editor"]["tab_width"])

    def rebuild_keys(self):
        self.keymap = build_keymap(self.cfg)

    def apply_timeout(self):
        secs = self.cfg["editor"]["autosave_seconds"]
        self.idle_ms = int(secs * 1000) if secs else -1
        self.stdscr.timeout(self.idle_ms)

    def apply_theme(self, name):
        self.cfg["theme"] = name
        try:
            theme = build_theme(self.cfg)
        except FileNotFoundError:
            self.message(f"no theme named {name}", error=True)
            return
        theme.apply()
        self.theme = theme
        self.message(f"theme: {name}")

    def reload_config(self):
        cfg, path, err = cfgmod.load(self.config_path)
        self.cfg = cfg
        self.rebuild_keys()
        self.apply_timeout()
        self.apply_theme(cfg["theme"])
        self.message(err or "config reloaded (plugins need a restart)", error=bool(err))

    def read_key(self):
        return read_key(self.stdscr, self.idle_ms)

    # ----- messages and events -----

    def message(self, text, error=False):
        self.msg = text
        self.msg_error = error

    def emit(self, event, *args):
        for fn in self.hooks.get(event, []):
            try:
                fn(self, *args)
            except Exception as e:
                self.message(f"plugin hook {event}: {e}", error=True)

    def tick(self):
        """Called every loop. Handles autosave."""
        secs = self.cfg["editor"]["autosave_seconds"]
        if secs and self.buf.dirty and self.buf.path:
            if time.monotonic() - self.last_change >= secs:
                self.save()

    # ----- undo / redo -----

    def record(self, group=None):
        """Call before an edit. Consecutive edits in the same group share one undo step."""
        if group is None or group != self._group:
            self.undo_stack.append(self.buf.snapshot())
            limit = int(self.cfg["editor"]["undo_levels"])
            if len(self.undo_stack) > limit:
                del self.undo_stack[: len(self.undo_stack) - limit]
            self.redo_stack.clear()
        self._group = group
        self.last_change = time.monotonic()

    def break_group(self):
        self._group = None

    def undo(self):
        if not self.undo_stack:
            self.message("nothing to undo")
            return
        self.redo_stack.append(self.buf.snapshot())
        self.buf.restore(self.undo_stack.pop())
        self._group = None

    def redo(self):
        if not self.redo_stack:
            self.message("nothing to redo")
            return
        self.undo_stack.append(self.buf.snapshot())
        self.buf.restore(self.redo_stack.pop())
        self._group = None

    def type_char(self, ch):
        self.record("type")
        self.buf.delete_selection()
        self.buf.anchor = None
        self.buf.insert(ch)

    # ----- files -----

    def save(self, path=None):
        cfg = self.cfg["editor"]
        try:
            written = self.buf.save(path, trim=cfg["trim_trailing_spaces"],
                                    final_newline=cfg["final_newline"])
            self.quit_armed = False
            self.message(f"wrote {written}")
            self.emit("save", written)
        except (ValueError, OSError) as e:
            self.message(str(e), error=True)

    def open_file(self, path):
        if self.buf.dirty:
            self.message("unsaved changes: save first", error=True)
            return
        self.buf = Buffer.open(str(path))
        self.top = self.left = 0
        self.undo_stack.clear()
        self.redo_stack.clear()
        self._group = None
        self.message(f"opened {path}")
        self.emit("open", str(path))

    def request_quit(self):
        if not self.buf.dirty or self.quit_armed:
            self.should_quit = True
        else:
            self.quit_armed = True
            self.message("unsaved changes: press Ctrl+X again to quit", error=True)

    # ----- search -----

    def find(self, needle):
        pos = self.buf.find(needle, self.buf.row, self.buf.col)
        if pos is None:
            self.message(f"not found: {needle}", error=True)
            return False
        self.buf.move_to(*pos)
        self.buf.anchor = None
        self.message(f"found: {needle}")
        return True

    def goto(self, n):
        self.buf.move_to(n - 1, 0)
        self.buf.anchor = None
        self.message(f"line {self.buf.row + 1}")

    # ----- prompt on the bottom row -----

    def draw_prompt(self, text):
        h, w = self.stdscr.getmaxyx()
        shown = text[: w - 1]
        self.stdscr.addnstr(h - 1, 0, shown.ljust(w - 1), w - 1, self.theme.attr("prompt"))
        self.stdscr.move(h - 1, min(len(shown), w - 2))
        self.stdscr.refresh()

    def prompt(self, label, initial=""):
        """Read a line on the bottom row. Returns the text, or None on Esc."""
        text = initial
        while True:
            self.draw_prompt(label + text)
            name, ch = self.read_key()
            if name == "esc":
                return None
            if name == "enter":
                return text
            if name == "backspace":
                text = text[:-1]
            elif ch:
                text += ch

    # ----- layout -----

    def geometry(self):
        h, w = self.stdscr.getmaxyx()
        v = self.cfg["view"]
        top_row = 1 if v["show_header"] else 0
        text_h = max(1, h - top_row - 1)
        mode = v["line_numbers"] if v["line_numbers"] in CYCLE + ["none"] else "left"
        width = v["line_number_width"]
        if width == "auto":
            digits = max(1, len(str(len(self.buf.lines))))
        else:
            digits = max(1, int(width))
        gutter = 0 if mode == "none" else digits + 2
        text_w = max(1, w - gutter)
        return {"h": h, "w": w, "top_row": top_row, "text_h": text_h,
                "mode": mode, "digits": digits, "gutter": gutter, "text_w": text_w}

    def disp_x(self, line, col):
        """Display column of a character index, with tabs expanded."""
        return len(line[:col].expandtabs(self.tab))

    def _scroll(self, g):
        b = self.buf
        margin = min(int(self.cfg["editor"]["scroll_margin"]), (g["text_h"] - 1) // 2)
        if b.row < self.top + margin:
            self.top = max(0, b.row - margin)
        if b.row >= self.top + g["text_h"] - margin:
            self.top = b.row - g["text_h"] + margin + 1
        x = self.disp_x(b.line, b.col)
        if x < self.left:
            self.left = x
        elif x >= self.left + g["text_w"]:
            self.left = x - g["text_w"] + 1

    # ----- drawing -----

    def draw(self):
        g = self.geometry()
        self._scroll(g)
        self.stdscr.erase()
        self._draw_header(g)
        self._draw_text(g)
        self._draw_message(g)
        self._place_cursor(g)
        self.stdscr.refresh()

    def _draw_header(self, g):
        if not self.cfg["view"]["show_header"]:
            return
        b = self.buf
        fields = {
            "name": b.path or "[new]",
            "modified": " *" if b.dirty else "",
            "mark": " [mark]" if b.anchor is not None else "",
            "row": b.row + 1,
            "col": self.disp_x(b.line, b.col) + 1,
            "lines": len(b.lines),
        }
        v = self.cfg["view"]
        try:
            left = v["header_left"].format(**fields)
            right = v["header_right"].format(**fields)
        except (KeyError, IndexError, ValueError):
            left = f" neonano  {fields['name']}"
            right = f"{fields['row']}:{fields['col']} "
        w = g["w"]
        pad = max(1, w - 1 - len(left) - len(right))
        header = (left + " " * pad + right)[: w - 1]
        self.stdscr.addnstr(0, 0, header.ljust(w - 1), w - 1, self.theme.attr("header"))

    def _draw_text(self, g):
        scr = self.stdscr
        v = self.cfg["view"]
        b = self.buf
        sel = b.selection()
        tx0 = g["gutter"] if g["mode"] == "left" else 0
        gx = 0 if g["mode"] == "left" else g["text_w"]
        text_w = g["text_w"]
        plain = self.theme.attr("text")
        cur_attr = self.theme.attr("current_line")
        sel_attr = self.theme.attr("selection")
        sep = v["gutter_separator"][:1] or "|"
        digits = g["digits"]

        for y in range(g["text_h"]):
            idx = self.top + y
            sy = g["top_row"] + y
            if idx >= len(b.lines):
                break
            line = b.lines[idx]
            exp = line.expandtabs(self.tab)
            base = cur_attr if (v["current_line"] and idx == b.row) else plain
            scr.addnstr(sy, tx0, " " * text_w, text_w, base)
            scr.addnstr(sy, tx0, exp[self.left:self.left + text_w], text_w, base)

            if sel and sel[0][0] <= idx <= sel[1][0]:
                (r1, c1), (r2, c2) = sel
                s = c1 if idx == r1 else 0
                e = c2 if idx == r2 else len(line)
                x1 = self.disp_x(line, s)
                x2 = self.disp_x(line, e) + (1 if idx != r2 else 0)
                x1 = max(x1, self.left)
                x2 = min(x2, self.left + text_w)
                if x2 > x1:
                    seg = exp.ljust(x2)[x1:x2]
                    scr.addnstr(sy, tx0 + x1 - self.left, seg, x2 - x1, sel_attr)

            if g["gutter"]:
                if v["relative_numbers"] and idx != b.row:
                    num = abs(idx - b.row)
                else:
                    num = idx + 1
                gattr = self.theme.attr("gutter_current" if idx == b.row else "gutter")
                if g["mode"] == "left":
                    txt = f"{num:>{digits}} {sep}"
                else:
                    txt = f"{sep} {num:<{digits}}"
                scr.addnstr(sy, gx, txt, g["gutter"], gattr)

    def _draw_message(self, g):
        attr = self.theme.attr("error" if self.msg_error else "status")
        self.stdscr.addnstr(g["h"] - 1, 0, self.msg.ljust(g["w"] - 1), g["w"] - 1, attr)

    def _place_cursor(self, g):
        b = self.buf
        tx0 = g["gutter"] if g["mode"] == "left" else 0
        y = g["top_row"] + (b.row - self.top)
        x = tx0 + self.disp_x(b.line, b.col) - self.left
        if 0 <= y < g["h"] - 1 and 0 <= x < g["w"]:
            self.stdscr.move(y, x)
