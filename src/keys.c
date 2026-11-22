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
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "keys.h"

/* Action names as used in [keys]. Order must match the enum. */
static const char *act_names[ACT_COUNT] = {
    "",
    "palette", "save", "quit",
    "undo", "redo",
    "find", "find_next", "goto_line",
    "mark", "select_all", "cut", "copy", "paste",
    "delete_line", "duplicate_line",
    "cancel", "redraw",
    "move_left", "move_right", "move_up", "move_down",
    "word_left", "word_right",
    "line_start", "line_end", "buffer_start", "buffer_end",
    "page_up", "page_down",
    "insert_newline", "delete_back", "delete_forward", "indent", "unindent",
    "toggle_line_numbers", "toggle_relative", "toggle_current_line",
    "cycle_theme", "reload_config", "open_config", "word_count",
};

typedef struct { int key; Action act; } Binding;

static const Binding defaults[] = {
    { KEY_F(1),                ACT_PALETTE },
    { 19,                      ACT_SAVE },
    { 24,                      ACT_QUIT },
    { 26,                      ACT_UNDO },
    { 25,                      ACT_REDO },
    { 6,                       ACT_FIND },
    { KEY_F(3),                ACT_FIND_NEXT },
    { 7,                       ACT_GOTO },
    { 20,                      ACT_MARK },
    { KEY_ALT_FLAG | 'a',      ACT_SELECT_ALL },
    { 11,                      ACT_CUT },
    { KEY_ALT_FLAG | 'c',      ACT_COPY },
    { 21,                      ACT_PASTE },
    { 4,                       ACT_DELETE_LINE },
    { KEY_ALT_FLAG | 'd',      ACT_DUPLICATE },
    { 27,                      ACT_CANCEL },
    { 12,                      ACT_REDRAW },
    { KEY_LEFT,                ACT_LEFT },
    { KEY_RIGHT,               ACT_RIGHT },
    { KEY_UP,                  ACT_UP },
    { KEY_DOWN,                ACT_DOWN },
    { KEY_ALT_FLAG | 'b',      ACT_WORD_LEFT },
    { KEY_ALT_FLAG | 'f',      ACT_WORD_RIGHT },
    { KEY_HOME,                ACT_HOME },
    { 1,                       ACT_HOME },
    { KEY_END,                 ACT_END },
    { 5,                       ACT_END },
    { KEY_ALT_FLAG | '<',      ACT_BUF_START },
    { KEY_ALT_FLAG | '>',      ACT_BUF_END },
    { KEY_PPAGE,               ACT_PAGE_UP },
    { KEY_NPAGE,               ACT_PAGE_DOWN },
    { 10,                      ACT_NEWLINE },
    { 127,                     ACT_BACKSPACE },
    { 8,                       ACT_BACKSPACE },
    { KEY_DC,                  ACT_DELETE },
    { 9,                       ACT_INDENT },
    { KEY_BTAB,                ACT_UNINDENT },
    { KEY_ALT_FLAG | 'n',      ACT_TOGGLE_NUMBERS },
    { KEY_ALT_FLAG | 'l',      ACT_TOGGLE_RELATIVE },
    { KEY_ALT_FLAG | 'h',      ACT_TOGGLE_CURLINE },
    { KEY_ALT_FLAG | 't',      ACT_CYCLE_THEME },
    { KEY_ALT_FLAG | 'k',      ACT_RELOAD },
    { KEY_ALT_FLAG | 'i',      ACT_OPEN_CONFIG },
    { KEY_ALT_FLAG | 'w',      ACT_WORD_COUNT },
};

#define MAX_BIND 256
static Binding binds[MAX_BIND];
static int nbinds;

static void bind_remove_key(int key) {
    int j = 0;
    for (int i = 0; i < nbinds; i++)
        if (binds[i].key != key) binds[j++] = binds[i];
    nbinds = j;
}

static void bind_remove_action(Action a) {
    int j = 0;
    for (int i = 0; i < nbinds; i++)
        if (binds[i].act != a) binds[j++] = binds[i];
    nbinds = j;
}

/* A key does one thing: binding it again moves it to the new action. */
static void bind_add(int key, Action a) {
    bind_remove_key(key);
    if (nbinds < MAX_BIND) {
        binds[nbinds].key = key;
        binds[nbinds].act = a;
        nbinds++;
    }
}

static Action action_by_name(const char *name) {
    for (int i = 1; i < ACT_COUNT; i++)
        if (!strcmp(act_names[i], name)) return (Action)i;
    return ACT_NONE;
}

static const struct { const char *name; int key; } named[] = {
    { "tab", 9 },          { "enter", 10 },       { "esc", 27 },
    { "backspace", 127 },  { "delete", KEY_DC },  { "insert", KEY_IC },
    { "home", KEY_HOME },  { "end", KEY_END },
    { "pageup", KEY_PPAGE }, { "pagedown", KEY_NPAGE },
    { "up", KEY_UP },      { "down", KEY_DOWN },
    { "left", KEY_LEFT },  { "right", KEY_RIGHT },
    { "backtab", KEY_BTAB },
};

/* Parses "ctrl+x", "alt+c", "f1", "tab", "home", ... Returns -1 if invalid. */
int keys_parse_name(const char *s) {
    if (!strcmp(s, "ctrl+space")) return 0;   /* terminals send NUL */
    int mods = 0;                           /* 0 none, 1 ctrl, 2 alt */
    if (!strncmp(s, "ctrl+", 5))     { mods = 1; s += 5; }
    else if (!strncmp(s, "alt+", 4)) { mods = 2; s += 4; }

    size_t n = strlen(s);
    if (n == 1) {
        unsigned char c = (unsigned char)s[0];
        if (mods == 1) return isalpha(c) ? tolower(c) - 'a' + 1 : -1;
        if (mods == 2) return KEY_ALT_FLAG | c;
        return -1;                          /* plain printable keys are text */
    }
    if (mods != 0) return -1;

    for (size_t i = 0; i < sizeof named / sizeof named[0]; i++)
        if (!strcmp(s, named[i].name)) return named[i].key;

    if (s[0] == 'f' && n >= 2 && n <= 3) {
        int num = atoi(s + 1);
        if (num >= 1 && num <= 12) return KEY_F(num);
    }
    return -1;
}

const char *keymap_setup(void) {
    static char err[160];
    err[0] = '\0';

    nbinds = 0;
    for (size_t i = 0; i < sizeof defaults / sizeof defaults[0]; i++)
        bind_add(defaults[i].key, defaults[i].act);

    bool reset[ACT_COUNT] = { false };
    for (int i = 0; i < cfg.nkeys; i++) {
        Action a = action_by_name(cfg.keys[i].action);
        if (a == ACT_NONE) continue;        /* snippet_prompt etc: not implemented */

        if (!reset[a]) {                    /* the user's list replaces the defaults */
            reset[a] = true;
            bind_remove_action(a);
        }
        if (!cfg.keys[i].key[0]) continue;

        int k = keys_parse_name(cfg.keys[i].key);
        if (k < 0) {
            if (!err[0])
                snprintf(err, sizeof err, "keys: bad key '%s' for %s",
                         cfg.keys[i].key, cfg.keys[i].action);
            continue;
        }
        bind_add(k, a);
    }
    return err[0] ? err : NULL;
}

Action keymap_lookup(int key) {
    for (int i = 0; i < nbinds; i++)
        if (binds[i].key == key) return binds[i].act;
    return ACT_NONE;
}
