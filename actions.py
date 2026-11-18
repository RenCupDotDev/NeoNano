"""Built-in actions. Keybinds in config.toml [keys] point at these names.
Plugins add their own with api.action(name)."""
from commands import run_palette
from theme import list_themes
from editor import CYCLE

ACTIONS = {}


def action(name):
    def register(fn):
        ACTIONS[name] = fn
        return fn
    return register


def _nav(name, fn):
    """Movement: breaks the undo group, moves the cursor. An active mark extends the selection."""
    def run(ed):
        ed.break_group()
        fn(ed)
    ACTIONS[name] = run


_nav("move_left", lambda ed: ed.buf.move_left())
_nav("move_right", lambda ed: ed.buf.move_right())
_nav("move_up", lambda ed: ed.buf.move_up())
_nav("move_down", lambda ed: ed.buf.move_down())
_nav("line_start", lambda ed: ed.buf.line_start())
_nav("line_end", lambda ed: ed.buf.line_end())
_nav("word_left", lambda ed: ed.buf.word_left())
_nav("word_right", lambda ed: ed.buf.word_right())
_nav("buffer_start", lambda ed: ed.buf.move_to(0, 0))
_nav("buffer_end", lambda ed: ed.buf.move_to(len(ed.buf.lines) - 1, 10**9))
_nav("page_up", lambda ed: ed.buf.move_to(ed.buf.row - ed.geometry()["text_h"] + 1, ed.buf.col))
_nav("page_down", lambda ed: ed.buf.move_to(ed.buf.row + ed.geometry()["text_h"] - 1, ed.buf.col))


@action("insert_newline")
def _(ed):
    ed.record("newline")
    ed.buf.delete_selection()
    ed.buf.newline(auto_indent=ed.cfg["editor"]["auto_indent"])
    ed.buf.anchor = None


@action("delete_back")
def _(ed):
    ed.record("delete")
    if not ed.buf.delete_selection():
        ed.buf.backspace()
    ed.buf.anchor = None


@action("delete_forward")
def _(ed):
    ed.record("delete")
    if not ed.buf.delete_selection():
        ed.buf.delete()
    ed.buf.anchor = None


@action("indent")
def _(ed):
    ed.record("indent")
    prefix = " " * ed.tab if ed.cfg["editor"]["use_spaces"] else "\t"
    if ed.buf.selection():
        ed.buf.indent_rows(prefix)
    else:
        ed.buf.insert(prefix)
        ed.buf.anchor = None


@action("unindent")
def _(ed):
    ed.record("indent")
    ed.buf.unindent_rows(ed.tab)


@action("delete_line")
def _(ed):
    ed.record(None)
    ed.buf.delete_line()


@action("duplicate_line")
def _(ed):
    ed.record(None)
    ed.buf.duplicate_line()


@action("cut")
def _(ed):
    sel = ed.buf.selection()
    if not sel:
        ed.message("nothing selected", error=True)
        return
    ed.clipboard = ed.buf.text_in(*sel)
    ed.record(None)
    ed.buf.delete_range(*sel)
    ed.message(f"cut {len(ed.clipboard)} chars")


@action("copy")
def _(ed):
    sel = ed.buf.selection()
    if not sel:
        ed.message("nothing selected", error=True)
        return
    ed.clipboard = ed.buf.text_in(*sel)
    ed.buf.anchor = None
    ed.message(f"copied {len(ed.clipboard)} chars")


@action("paste")
def _(ed):
    if not ed.clipboard:
        ed.message("clipboard is empty", error=True)
        return
    ed.record(None)
    ed.buf.delete_selection()
    ed.buf.insert_text(ed.clipboard)
    ed.buf.anchor = None


@action("mark")
def _(ed):
    if ed.buf.anchor is None:
        ed.buf.anchor = (ed.buf.row, ed.buf.col)
        ed.message("mark set: move to select")
    else:
        ed.buf.anchor = None
        ed.message("mark cleared")


@action("select_all")
def _(ed):
    ed.buf.select_all()


@action("cancel")
def _(ed):
    ed.buf.anchor = None
    ed.quit_armed = False
    ed.message("")


@action("undo")
def _(ed):
    ed.undo()


@action("redo")
def _(ed):
    ed.redo()


@action("save")
def _(ed):
    ed.save()


@action("quit")
def _(ed):
    ed.request_quit()


@action("find")
def _(ed):
    needle = ed.prompt("find: ", ed.last_needle)
    if needle:
        ed.last_needle = needle
        ed.find(needle)


@action("find_next")
def _(ed):
    if ed.last_needle:
        ed.find(ed.last_needle)
    else:
        ed.message("no previous search", error=True)


@action("goto_line")
def _(ed):
    text = ed.prompt("line: ")
    if text and text.strip().isdigit():
        ed.goto(int(text))


@action("palette")
def _(ed):
    run_palette(ed)


@action("toggle_line_numbers")
def _(ed):
    cur = ed.cfg["view"]["line_numbers"]
    nxt = CYCLE[(CYCLE.index(cur) + 1) % len(CYCLE)] if cur in CYCLE else "left"
    ed.cfg["view"]["line_numbers"] = nxt
    ed.message(f"line numbers: {nxt}")


@action("toggle_relative")
def _(ed):
    v = ed.cfg["view"]
    v["relative_numbers"] = not v["relative_numbers"]
    ed.message(f"relative numbers: {v['relative_numbers']}")


@action("toggle_current_line")
def _(ed):
    v = ed.cfg["view"]
    v["current_line"] = not v["current_line"]
    ed.message(f"current line highlight: {v['current_line']}")


@action("cycle_theme")
def _(ed):
    names = list_themes()
    cur = ed.cfg["theme"]
    nxt = names[(names.index(cur) + 1) % len(names)] if cur in names else names[0]
    ed.apply_theme(nxt)


@action("reload_config")
def _(ed):
    ed.reload_config()


@action("open_config")
def _(ed):
    ed.open_file(ed.config_path)


@action("redraw")
def _(ed):
    ed.stdscr.clear()
