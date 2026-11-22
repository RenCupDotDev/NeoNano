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
#include "commands.h"
#include "config.h"
#include "keys.h"
#include "plugin.h"
#include "theme.h"
#include "undo.h"

#define MAX_CMDS 128

typedef struct {
    char  name[32];
    char  help[96];     /* empty for aliases, which are hidden from help */
    CmdFn fn;
} Command;

static Command cmds[MAX_CMDS];
static int     ncmds;

static Command *find_cmd(const char *name) {
    for (int i = 0; i < ncmds; i++)
        if (!strcmp(cmds[i].name, name)) return &cmds[i];
    return NULL;
}

bool cmd_register(const char *name, const char *help, CmdFn fn) {
    if (!name || !*name || !fn || ncmds >= MAX_CMDS) return false;
    if (strlen(name) >= sizeof cmds[0].name) return false;
    if (find_cmd(name)) return false;
    Command *c = &cmds[ncmds++];
    snprintf(c->name, sizeof c->name, "%s", name);
    snprintf(c->help, sizeof c->help, "%s", help ? help : "");
    c->fn = fn;
    return true;
}

/* ---------- shared actions ---------- */

static void do_open(Editor *e, const char *arg, bool force) {
    if (e->dirty && !force) {
        set_msg(e, "Unsaved changes: write first, or use open! / cfg!");
        return;
    }
    if (*arg) { open_file(e, arg); return; }
    char buf[256] = "";
    if (!prompt("File to open: ", buf, sizeof buf) || !buf[0]) {
        set_msg(e, "Cancelled");
        return;
    }
    open_file(e, buf);
}

static void do_exit(Editor *e, bool force) {
    if (e->dirty && !force) {
        set_msg(e, "Unsaved changes: write first, or use exit!");
        return;
    }
    e->quit = true;
}

void cmd_open_config(Editor *e) {
    do_open(e, config_file(), false);
}

void reload_all(Editor *e) {
    config_reset();
    const char *a = config_load();
    const char *b = theme_load(cfg.theme);
    const char *c = keymap_setup();
    theme_init();
    const char *err = a ? a : b ? b : c;
    if (err) set_msg(e, "%s", err);
    else     set_msg(e, "Config reloaded. Plugin changes need a restart");
}

void cmd_word_count(Editor *e) {
    size_t lines = e->buf.n, words = 0, bytes = 0;
    for (size_t r = 0; r < lines; r++) {
        const Line *l = &e->buf.lines[r];
        bytes += l->len;
        bool in = false;
        for (size_t i = 0; i < l->len; i++) {
            bool sp = isspace((unsigned char)l->s[i]);
            if (!sp && !in) { words++; in = true; }
            else if (sp)    { in = false; }
        }
    }
    set_msg(e, "%zu lines, %zu words, %zu bytes", lines, words, bytes);
}

/* ---------- palette commands ---------- */

static void c_write(Editor *e, const char *arg) {
    if (*arg) save_as(e, arg);
    else      save_file(e);
}

static void c_saveas(Editor *e, const char *arg) {
    if (*arg) { save_as(e, arg); return; }
    char buf[256] = "";
    if (e->path) snprintf(buf, sizeof buf, "%s", e->path);
    if (!prompt("File Name to Write: ", buf, sizeof buf) || !buf[0]) {
        set_msg(e, "Cancelled");
        return;
    }
    save_as(e, buf);
}

static void c_open(Editor *e, const char *arg)       { do_open(e, arg, false); }
static void c_open_force(Editor *e, const char *arg) { do_open(e, arg, true); }

static void c_read(Editor *e, const char *arg) {
    undo_checkpoint(e, UNDO_NONE);
    if (*arg) { insert_file(e, arg); return; }
    char buf[256] = "";
    if (!prompt("File to insert: ", buf, sizeof buf) || !buf[0]) {
        set_msg(e, "Cancelled");
        return;
    }
    insert_file(e, buf);
}

static void c_exit(Editor *e, const char *arg)       { (void)arg; do_exit(e, false); }
static void c_exit_force(Editor *e, const char *arg) { (void)arg; do_exit(e, true); }

static void c_find(Editor *e, const char *arg) {
    if (*arg) {
        snprintf(e->search, sizeof e->search, "%s", arg);
    } else {
        char buf[256];
        snprintf(buf, sizeof buf, "%s", e->search);
        if (!prompt("Search: ", buf, sizeof buf)) return;
        snprintf(e->search, sizeof e->search, "%s", buf);
    }
    if (!e->search[0]) { set_msg(e, "Nothing to search for"); return; }
    if (!find_next(e, true)) set_msg(e, "Not found: %s", e->search);
}

static void c_goto(Editor *e, const char *arg) {
    if (*arg) { goto_line(e, arg); return; }
    char buf[32] = "";
    if (!prompt("Go to line: ", buf, sizeof buf) || !buf[0]) {
        set_msg(e, "Cancelled");
        return;
    }
    goto_line(e, buf);
}

static void c_cut(Editor *e, const char *arg) {
    (void)arg;
    undo_checkpoint(e, UNDO_NONE);
    if (has_sel(e)) cut_sel(e);
    else            cut_line(e);
}

static void c_copy(Editor *e, const char *arg)  { (void)arg; copy_sel(e); }
static void c_mark(Editor *e, const char *arg)  { (void)arg; toggle_mark(e); }

static void c_paste(Editor *e, const char *arg) {
    (void)arg;
    undo_checkpoint(e, UNDO_NONE);
    paste(e);
}

static void c_deleteline(Editor *e, const char *arg) {
    (void)arg;
    undo_checkpoint(e, UNDO_NONE);
    delete_line(e);
}

/* :discard - throw away unsaved changes and reload the saved file. */
static void c_discard(Editor *e, const char *arg) {
    (void)arg;
    if (!e->dirty) {
        set_msg(e, "No unsaved changes");
        return;
    }
    char answer[8] = "";
    if (!prompt(e->path ? "Discard all changes and reload the file? (y/n): "
                        : "Discard all text in this new buffer? (y/n): ", answer, sizeof answer)
        || (strcmp(answer, "y") != 0 && strcmp(answer, "yes") != 0)) {
        set_msg(e, "Kept your changes");
        return;
    }
    if (e->path) {
        open_file(e, e->path);                 /* reloads from disk, clears undo */
        set_msg(e, "Changes discarded");
    } else {
        buf_free(&e->buf);
        buf_insert_line(&e->buf, 0, line_new());
        e->row = e->col = e->top = 0;
        e->left = 0;
        e->marking = false;
        e->dirty = false;
        undo_clear();
        e->version++;
        set_msg(e, "Buffer cleared");
    }
}

static void c_undo(Editor *e, const char *arg) {
    (void)arg;
    if (!undo_do(e)) set_msg(e, "Nothing to undo");
}

static void c_redo(Editor *e, const char *arg) {
    (void)arg;
    if (!redo_do(e)) set_msg(e, "Nothing to redo");
}

static void c_theme(Editor *e, const char *arg) {
    const char *err = *arg ? theme_load(arg) : theme_cycle();
    theme_init();
    if (err) set_msg(e, "%s", err);
    else     set_msg(e, "Theme: %s", cfg.theme);
}

static void c_cfg(Editor *e, const char *arg)       { (void)arg; do_open(e, config_file(), false); }
static void c_cfg_force(Editor *e, const char *arg) { (void)arg; do_open(e, config_file(), true); }

static void c_reload(Editor *e, const char *arg)  { (void)arg; reload_all(e); }
static void c_wc(Editor *e, const char *arg)      { (void)arg; cmd_word_count(e); }
static void c_plugins(Editor *e, const char *arg) { (void)arg; plugins_list(e); }

static void c_help(Editor *e, const char *arg) {
    if (*arg) {
        Command *c = find_cmd(arg);
        if (!c) { set_msg(e, "No such command: %s", arg); return; }
        set_msg(e, "%s", c->help[0] ? c->help : c->name);
        return;
    }
    char out[MSG_MAX] = "commands:";
    for (int i = 0; i < ncmds; i++) {
        if (!cmds[i].help[0]) continue;
        size_t used = strlen(out);
        if (sizeof out - used < 2) break;
        snprintf(out + used, sizeof out - used, " %s", cmds[i].name);
    }
    set_msg(e, "%s", out);
}

void commands_init(void) {
    cmd_register("write",      "write [file]  save the file (to file if given)", c_write);
    cmd_register("save",       NULL, c_write);
    cmd_register("w",          NULL, c_write);
    cmd_register("saveas",     "saveas [file]  save under a new name", c_saveas);
    cmd_register("open",       "open <file>  open a file (refused if unsaved)", c_open);
    cmd_register("open!",      "open! <file>  open a file, discarding changes", c_open_force);
    cmd_register("read",       "read <file>  insert a file at the cursor", c_read);
    cmd_register("exit",       "exit  quit (refused if unsaved)", c_exit);
    cmd_register("exit!",      "exit!  quit, discarding changes", c_exit_force);
    cmd_register("quit",       NULL, c_exit);
    cmd_register("q",          NULL, c_exit);
    cmd_register("discard",    "discard  throw away unsaved changes (asks first)", c_discard);
    cmd_register("undo",       "undo  undo the last change", c_undo);
    cmd_register("redo",       "redo  redo the last undone change", c_redo);
    cmd_register("find",       "find [text]  search forward from the cursor", c_find);
    cmd_register("whereis",    NULL, c_find);
    cmd_register("goto",       "goto <line>  jump to a line number", c_goto);
    cmd_register("cut",        "cut  cut the selection, or the current line", c_cut);
    cmd_register("copy",       "copy  copy the selection", c_copy);
    cmd_register("paste",      "paste  paste the cut or copied text", c_paste);
    cmd_register("mark",       "mark  set or clear the selection mark", c_mark);
    cmd_register("deleteline", "deleteline  delete the current line", c_deleteline);
    cmd_register("theme",      "theme [name]  switch theme (no name: next one)", c_theme);
    cmd_register("reload",     "reload  reload config.toml and themes", c_reload);
    cmd_register("wc",         "wc  count lines, words and bytes", c_wc);
    cmd_register("cfg",        "cfg  open config.toml", c_cfg);
    cmd_register("cfg!",       "cfg!  open config.toml, discarding changes", c_cfg_force);
    cmd_register("plugins",    "plugins  list loaded plugins", c_plugins);
    cmd_register("help",       "help [command]  list commands, or show one's usage", c_help);
}

/* ---------- palette ---------- */

void run_command_line(Editor *e, const char *text) {
    char line[256];
    snprintf(line, sizeof line, "%s", text);

    char *p = line;
    while (*p == ' ' || *p == '\t') p++;

    size_t n = strcspn(p, " \t");
    if (n == 0) return;
    if (n >= 32) { set_msg(e, "Unknown command"); return; }

    char name[32];
    memcpy(name, p, n);
    name[n] = '\0';

    char *arg = p + n;
    while (*arg == ' ' || *arg == '\t') arg++;
    size_t al = strlen(arg);
    while (al > 0 && isspace((unsigned char)arg[al - 1])) arg[--al] = '\0';

    Command *c = find_cmd(name);
    if (!c) { set_msg(e, "Unknown command: %s", name); return; }
    c->fn(e, arg);
}

void run_palette(Editor *e) {
    char line[256] = "";
    if (!prompt(":", line, sizeof line)) return;
    run_command_line(e, line);
}
