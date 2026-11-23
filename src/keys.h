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
#ifndef KEYS_H
#define KEYS_H

/* Set on a key id for alt+<char>. */
#define KEY_ALT_FLAG 0x200000

/* Ctrl+arrow and ctrl+shift+arrow: base + dir, dir 0 up, 1 down, 2 right, 3 left. */
#define KEY_CTRL_ARROW   0x300000
#define KEY_CSHIFT_ARROW 0x300010

typedef enum {
    ACT_NONE,
    ACT_PALETTE, ACT_SAVE, ACT_QUIT,
    ACT_UNDO, ACT_REDO,
    ACT_FIND, ACT_FIND_NEXT, ACT_GOTO,
    ACT_MARK, ACT_SELECT_ALL, ACT_CUT, ACT_COPY, ACT_PASTE,
    ACT_DELETE_LINE, ACT_DUPLICATE,
    ACT_CANCEL, ACT_REDRAW,
    ACT_LEFT, ACT_RIGHT, ACT_UP, ACT_DOWN,
    ACT_WORD_LEFT, ACT_WORD_RIGHT,
    ACT_HOME, ACT_END, ACT_BUF_START, ACT_BUF_END,
    ACT_PAGE_UP, ACT_PAGE_DOWN,
    ACT_NEWLINE, ACT_BACKSPACE, ACT_DELETE, ACT_INDENT, ACT_UNINDENT,
    ACT_TOGGLE_NUMBERS, ACT_TOGGLE_RELATIVE, ACT_TOGGLE_CURLINE,
    ACT_CYCLE_THEME, ACT_RELOAD, ACT_OPEN_CONFIG, ACT_WORD_COUNT,
    ACT_CLOSE_PANE,
    ACT_COUNT
} Action;

/* Builds the key table from the defaults plus cfg.keys.
 * Returns an error message for a bad key name, or NULL. */
const char *keymap_setup(void);

/* Returns the action bound to a key id, or ACT_NONE. */
Action keymap_lookup(int key);

/* Parses "ctrl+x", "alt+c", "f12", ... Returns -1 if invalid. */
int keys_parse_name(const char *s);

#endif
