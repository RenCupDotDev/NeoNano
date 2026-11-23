/*
 * Copyright (C) 2026 RenCupDotDev
 *
 * This file is part of neonano.
 *
 * neonano is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * neonano is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "editor.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "commands.h"
#include "keys.h"
#include "layout.h"
#include "plugin.h"
#include "popup.h"
#include "theme.h"
#include "undo.h"
#include "utf8.h"

static bool nonblock;

/* ctrl and ctrl+shift arrows come from ncurses under terminfo names
 * kUP5..kRIT5 (ctrl) and kUP6..kRIT6 (ctrl+shift). Returns -1 otherwise. */
static int arrow_mod_key(const char *name) {
    if (!name || name[0] != 'k') return -1;
    int dir = -1;
    if (!strncmp(name, "kUP", 3))        dir = 0;
    else if (!strncmp(name, "kDN", 3))   dir = 1;
    else if (!strncmp(name, "kRIT", 4))  dir = 2;
    else if (!strncmp(name, "kLFT", 4))  dir = 3;
    if (dir < 0) return -1;
    char last = name[strlen(name) - 1];
    if (last == '5') return KEY_CTRL_ARROW + dir;
    if (last == '6') return KEY_CSHIFT_ARROW + dir;
    return -1;
}

void input_drain_mode(bool on) {
    nonblock = on;
    timeout(on ? 0 : 100);       /* 100 ms idle wait lets plugins tick */
}

/* Reads one input event. Returns:
 *   INPUT_TEXT with the code point in *cp for printable text,
 *   a key id for control keys, special keys, alt+<char> (KEY_ALT_FLAG | char),
 *   ctrl and ctrl+shift arrows (KEY_CTRL_ARROW / KEY_CSHIFT_ARROW + dir),
 *   or INPUT_NONE on error. */
int next_key(wint_t *cp) {
    wint_t wc = 0;
    int r = get_wch(&wc);
    if (r == ERR) return INPUT_NONE;

    if (r == KEY_CODE_YES) {
        if (wc == KEY_BACKSPACE) return 127;
        if (wc == KEY_ENTER)     return 10;
        int m = arrow_mod_key(keyname((int)wc));
        if (m >= 0) return m;
        return (int)wc;
    }

    if (wc == 13) return 10;                 /* enter */

    if (wc == 27) {                          /* esc, alt+<char>, or an escape sequence */
        timeout(0);
        wint_t nx = 0;
        int r2 = get_wch(&nx);
        int ret = 27;
        if (r2 == OK && (nx == '[' || nx == 'O')) {
            /* Read the sequence up to its final byte, e.g. "1;5D" for ctrl+left. */
            char seq[16];
            size_t n = 0;
            for (;;) {
                wint_t c = 0;
                if (get_wch(&c) != OK) break;
                if (n < sizeof seq - 1) seq[n++] = (char)c;
                if (c >= 0x40 && c <= 0x7e) break;
            }
            seq[n] = '\0';
            if (n == 1) {                       /* plain arrows sent as ESC [ x */
                switch (seq[0]) {
                case 'A': ret = KEY_UP;    break;
                case 'B': ret = KEY_DOWN;  break;
                case 'C': ret = KEY_RIGHT; break;
                case 'D': ret = KEY_LEFT;  break;
                case 'H': ret = KEY_HOME;  break;
                case 'F': ret = KEY_END;   break;
                default:  break;
                }
            }
            if (n == 4 && seq[0] == '1' && seq[1] == ';') {
                const char *dirs = "ABCD";           /* up, down, right, left */
                for (int d = 0; d < 4; d++) {
                    if (seq[3] != dirs[d]) continue;
                    if (seq[2] == '5') ret = KEY_CTRL_ARROW + (d == 0 ? 0 : d == 1 ? 1 : d == 2 ? 2 : 3);
                    if (seq[2] == '6') ret = KEY_CSHIFT_ARROW + (d == 0 ? 0 : d == 1 ? 1 : d == 2 ? 2 : 3);
                }
            }
        } else if (r2 == OK && nx >= 32 && nx != 127) {
            ret = KEY_ALT_FLAG | (int)nx;
        } else if (r2 == OK) {
            unget_wch(nx);
        }
        timeout(nonblock ? 0 : 100);
        return ret;
    }

    if (wc < 32 || wc == 127) return (int)wc;
    *cp = wc;
    return INPUT_TEXT;
}

bool prompt(const char *label, char *buf, size_t cap) {
    bool was = nonblock;
    input_drain_mode(false);            /* prompts always block */
    bool ok = false;
    for (;;) {
        attrset(theme_attr(PAIR_PROMPT));
        move(LINES - 1, 0);
        clrtoeol();
        printw("%s%s", label, buf);
        attrset(A_NORMAL);
        refresh();

        wint_t cp = 0;
        int k = next_key(&cp);
        if (k == INPUT_NONE) continue;
        if (k == 10) { ok = true; break; }
        if (k == 27) break;

        size_t len = strlen(buf);
        if (k == 127 || k == 8) {
            if (len > 0) buf[utf8_prev(buf, len, len)] = '\0';
        } else if (k == INPUT_TEXT) {
            char enc[4];
            size_t n = utf8_encode((unsigned int)cp, enc);
            if (len + n + 1 <= cap) {
                memcpy(buf + len, enc, n);
                buf[len + n] = '\0';
            }
        }
    }
    input_drain_mode(was);
    return ok;
}

/* Edits get an undo checkpoint before they change anything. */
static bool is_edit(Action a) {
    switch (a) {
    case ACT_CUT: case ACT_PASTE: case ACT_DELETE_LINE: case ACT_DUPLICATE:
    case ACT_NEWLINE: case ACT_BACKSPACE: case ACT_DELETE:
    case ACT_INDENT: case ACT_UNINDENT:
        return true;
    default:
        return false;
    }
}

/* Consecutive edits in the same group share one undo step. */
static int edit_group(Action a) {
    switch (a) {
    case ACT_NEWLINE:                    return UNDO_TYPE;
    case ACT_BACKSPACE: case ACT_DELETE: return UNDO_DEL;
    default:                             return UNDO_NONE;
    }
}

/* Movement ends a typing group, so the next typed text is a new undo step. */
static bool is_move(Action a) {
    switch (a) {
    case ACT_LEFT: case ACT_RIGHT: case ACT_UP: case ACT_DOWN:
    case ACT_WORD_LEFT: case ACT_WORD_RIGHT:
    case ACT_HOME: case ACT_END: case ACT_BUF_START: case ACT_BUF_END:
    case ACT_PAGE_UP: case ACT_PAGE_DOWN: case ACT_SELECT_ALL:
        return true;
    default:
        return false;
    }
}

static void run_action(Editor *e, Action a) {
    bool was_armed = e->quit_armed;
    e->quit_armed = false;

    if (is_edit(a))      undo_checkpoint(e, edit_group(a));
    else if (is_move(a)) undo_break();

    int page = LINES - 2;
    if (page < 1) page = 1;

    switch (a) {
    case ACT_PALETTE:
        run_palette(e);
        break;

    case ACT_SAVE:
        save_file(e);
        break;

    case ACT_QUIT:
        if (e->dirty && !was_armed) {
            e->quit_armed = true;
            set_msg(e, "Unsaved changes. Press the quit key again to discard them");
        } else {
            e->quit = true;
        }
        break;

    case ACT_UNDO:
        if (!undo_do(e)) set_msg(e, "Nothing to undo");
        break;

    case ACT_REDO:
        if (!redo_do(e)) set_msg(e, "Nothing to redo");
        break;

    case ACT_FIND: {
        char q[256];
        snprintf(q, sizeof q, "%s", e->search);
        if (prompt("Search: ", q, sizeof q)) {
            snprintf(e->search, sizeof e->search, "%s", q);
            if (!find_next(e, true)) set_msg(e, "Not found: %s", e->search);
        }
        break;
    }

    case ACT_FIND_NEXT:
        if (!find_next(e, true)) set_msg(e, "Not found: %s", e->search);
        break;

    case ACT_GOTO: {
        char g[32] = "";
        if (prompt("Go to line: ", g, sizeof g)) goto_line(e, g);
        break;
    }

    case ACT_MARK:        toggle_mark(e);    break;
    case ACT_SELECT_ALL:  select_all(e);     break;

    case ACT_CUT:
        if (has_sel(e)) cut_sel(e);
        else            cut_line(e);
        break;

    case ACT_COPY:        copy_sel(e);       break;
    case ACT_PASTE:       paste(e);          break;
    case ACT_DELETE_LINE: delete_line(e);    break;
    case ACT_DUPLICATE:   duplicate_line(e); break;
    case ACT_CANCEL:      clear_mark(e);     break;
    case ACT_REDRAW:      clear();           break;
    case ACT_CLOSE_PANE:
        if (!layout_close_focused()) set_msg(e, "Nothing to close: the editor is the only pane");
        break;

    case ACT_LEFT:        move_left(e);      break;
    case ACT_RIGHT:       move_right(e);     break;
    case ACT_UP:          move_up(e);        break;
    case ACT_DOWN:        move_down(e);      break;
    case ACT_WORD_LEFT:   word_left(e);      break;
    case ACT_WORD_RIGHT:  word_right(e);     break;
    case ACT_HOME:        line_home(e);      break;
    case ACT_END:         line_end(e);       break;
    case ACT_BUF_START:   buffer_start(e);   break;
    case ACT_BUF_END:     buffer_end(e);     break;
    case ACT_PAGE_UP:     page_up(e, (size_t)page);   break;
    case ACT_PAGE_DOWN:   page_down(e, (size_t)page); break;

    case ACT_NEWLINE:     newline(e);        break;
    case ACT_BACKSPACE:   del_back(e);       break;
    case ACT_DELETE:      del_forward(e);    break;
    case ACT_INDENT:      indent_lines(e);   break;
    case ACT_UNINDENT:    unindent(e);       break;

    case ACT_TOGGLE_NUMBERS:
        cfg.line_numbers = cfg.line_numbers ? 0 : 1;
        set_msg(e, cfg.line_numbers ? "Line numbers on" : "Line numbers off");
        break;

    case ACT_TOGGLE_RELATIVE:
        cfg.relative_numbers = !cfg.relative_numbers;
        set_msg(e, cfg.relative_numbers ? "Relative numbers on" : "Relative numbers off");
        break;

    case ACT_TOGGLE_CURLINE:
        cfg.current_line = !cfg.current_line;
        theme_init();
        set_msg(e, cfg.current_line ? "Current line highlight on" : "Current line highlight off");
        break;

    case ACT_CYCLE_THEME: {
        const char *err = theme_cycle();
        theme_init();
        if (err) set_msg(e, "%s", err);
        else     set_msg(e, "Theme: %s", cfg.theme);
        break;
    }

    case ACT_RELOAD:      reload_all(e);       break;
    case ACT_OPEN_CONFIG: cmd_open_config(e);  break;
    case ACT_WORD_COUNT:  cmd_word_count(e);   break;

    default:
        break;
    }
}

/* Keys for the editor pane: popup, plugin bindings, then the keymap. */
static void editor_key(Editor *e, int key) {
    if (popup_key(e, key)) return;
    const char *pc = plugin_key_command(key);
    if (pc) { run_command_line(e, pc); return; }
    Action a = keymap_lookup(key);
    if (a != ACT_NONE) run_action(e, a);
}

void input_event(Editor *e, int key, unsigned int cp) {
    e->msg[0] = '\0';

    /* Layout keys work from any pane. */
    if (key >= KEY_CTRL_ARROW && key < KEY_CTRL_ARROW + 4) {
        layout_resize(key - KEY_CTRL_ARROW);
        return;
    }
    if (key >= KEY_CSHIFT_ARROW && key < KEY_CSHIFT_ARROW + 4) {
        layout_move(layout_area(), key - KEY_CSHIFT_ARROW);
        return;
    }
    if (key == KEY_F(9)) {
        layout_focus_next();
        return;
    }

    Pane *f = layout_focused();
    if (!f->is_editor) {                     /* a plugin pane has focus */
        int pk = (key == INPUT_TEXT) ? (int)(PLUGIN_KEY_TEXT | cp) : key;
        if (f->on_key && f->on_key(e, pk, f->ud)) return;
        if (pk == 27) {                               /* esc: close an overlay, or go to the editor */
            if (layout_is_overlay(f)) layout_close(f->id);
            else                      layout_focus_editor();
            return;
        }
        if (key == INPUT_TEXT) return;       /* a pane that ignores text ignores it */
        /* Keys the pane did not use still reach plugin bindings and the
         * global keys (palette, save, quit, find), so ctrl+b works from a pane. */
        const char *pc = plugin_key_command(key);
        if (pc) { run_command_line(e, pc); return; }
        Action a = keymap_lookup(key);
        /* quit is not global: inside a pane, exit with :exit from the palette */
        if (a == ACT_PALETTE || a == ACT_SAVE || a == ACT_FIND || a == ACT_CLOSE_PANE)
            run_action(e, a);
        return;
    }

    if (key == INPUT_TEXT) {
        popup_key(e, key);
        undo_checkpoint(e, UNDO_TYPE);
        type_codepoint(e, cp);
    } else {
        editor_key(e, key);
    }

    if (key == INPUT_TEXT || key == 127 || key == 8)
        popup_after(e);                      /* completion re-filters as you type */
}
