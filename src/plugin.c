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
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "config.h"
#include "decor.h"
#include "keys.h"
#include "layout.h"
#include "plugin.h"
#include "popup.h"
#include "undo.h"
#include "util.h"

static char loaded[MAX_PLUGINS][PLUGIN_NAME_LEN];
static int  nloaded;

static TickFn ticks[8];
static int    nticks;
static SaveFn saves[8];
static int    nsaves;

typedef struct { int key; char cmd[96]; } PluginKey;
static PluginKey pkeys[32];
static int       npkeys;

static bool host_add_command(const char *name, const char *help, CmdFn fn) {
    return cmd_register(name, help, fn);
}

static void host_message(Editor *e, const char *text) {
    set_msg(e, "%s", text);
}

static size_t host_line_count(const Editor *e) {
    return e->buf.n;
}

static const char *host_line_text(const Editor *e, size_t row, size_t *len) {
    if (row >= e->buf.n) { *len = 0; return ""; }
    *len = e->buf.lines[row].len;
    return e->buf.lines[row].s;
}

static const char *host_file_path(const Editor *e) {
    return e->path;
}

static void host_get_cursor(const Editor *e, size_t *row, size_t *col) {
    *row = e->row;
    *col = e->col;
}

static void host_set_cursor(Editor *e, size_t row, size_t col) {
    if (row >= e->buf.n) row = e->buf.n - 1;
    e->row = row;
    e->col = col;
    clamp_col(e);
}

static void host_on_tick(TickFn f) {
    if (nticks < 8) ticks[nticks++] = f;
}

static void host_on_save(SaveFn f) {
    if (nsaves < 8) saves[nsaves++] = f;
}

static bool host_bind_key(const char *key, const char *cmd) {
    int k = keys_parse_name(key);
    if (k < 0) return false;
    for (int i = 0; i < npkeys; i++)
        if (pkeys[i].key == k) {
            snprintf(pkeys[i].cmd, sizeof pkeys[i].cmd, "%s", cmd);
            return true;
        }
    if (npkeys >= 32) return false;
    pkeys[npkeys].key = k;
    snprintf(pkeys[npkeys].cmd, sizeof pkeys[npkeys].cmd, "%s", cmd);
    npkeys++;
    return true;
}

static unsigned long host_buffer_version(const Editor *e) {
    return e->version;
}

static void host_decor_clear(const char *owner) {
    decor_clear(owner);
}

static void host_decor_add(const char *owner, size_t row, size_t c0, size_t c1,
                           int kind, const char *msg) {
    decor_add(owner, row, c0, c1, kind, msg);
}

static void host_popup_open(Editor *e, const char *title, const char **lines, int n,
                            bool selectable, PopupPick pick, void *ud) {
    popup_open(e, title, lines, n, selectable, pick, ud);
}

static void host_popup_close(void) {
    popup_close();
}

static size_t clamp_byte(const Editor *e, size_t row, size_t col) {
    const Line *l = &e->buf.lines[row];
    if (col > l->len) col = l->len;
    while (col > 0 && col < l->len && ((unsigned char)l->s[col] & 0xC0) == 0x80)
        col--;
    return col;
}

/* Applies the edits in the order given, as one undo step. */
static void host_apply_edits(Editor *e, const TextEdit *ed, int n) {
    if (!e->buf.n || n <= 0) return;
    undo_checkpoint(e, UNDO_NONE);
    for (int i = 0; i < n; i++) {
        size_t r0 = ed[i].r0 < e->buf.n ? ed[i].r0 : e->buf.n - 1;
        size_t r1 = ed[i].r1 < e->buf.n ? ed[i].r1 : e->buf.n - 1;
        size_t c0 = clamp_byte(e, r0, ed[i].c0);
        size_t c1 = clamp_byte(e, r1, ed[i].c1);
        e->marking = true;
        e->mrow = r0;
        e->mcol = c0;
        e->row = r1;
        e->col = c1;
        const char *t = ed[i].text ? ed[i].text : "";
        insert_text(e, t, strlen(t));   /* removes the selection, then inserts */
    }
    e->marking = false;
}

static bool host_open_path(Editor *e, const char *path, size_t row, size_t col) {
    if (e->dirty) {
        set_msg(e, "save or discard changes before opening %s", path);
        return false;
    }
    if (!open_file(e, path)) return false;
    host_set_cursor(e, row, col);
    return true;
}

static bool host_prompt(const char *label, char *buf, size_t cap) {
    return prompt(label, buf, cap);
}

static int host_pane_open(const char *name, int side, int percent, PaneKeyFn fn, void *ud) {
    return layout_open(name, side, percent, fn, ud);
}

static void host_pane_close(int id) {
    layout_close(id);
}

static void host_pane_set_lines(int id, const char **lines, int n) {
    layout_set_lines(id, lines, n);
}

static void host_pane_set_cursor(int id, int row) {
    layout_set_cursor(id, row);
}

static void host_pane_focus(int id) {
    layout_focus_id(id);
}

static void host_popup_filterable(PopupFilter fn, void *ud) {
    popup_filterable(fn, ud);
}

static const char *host_config_get(const char *section, const char *key) {
    return config_get(section, key);
}

static void host_run_command(Editor *e, const char *command) {
    run_command_line(e, command);
}

static const PluginAPI host_api = {
    PLUGIN_API_VERSION,
    host_add_command,
    host_message,
    host_line_count,
    host_line_text,
    host_file_path,
    host_get_cursor,
    host_set_cursor,
    host_on_tick,
    host_on_save,
    host_bind_key,
    host_buffer_version,
    host_decor_clear,
    host_decor_add,
    host_popup_open,
    host_popup_close,
    host_apply_edits,
    host_open_path,
    host_prompt,
    host_pane_open,
    host_pane_close,
    host_pane_set_lines,
    host_pane_set_cursor,
    host_pane_focus,
    host_popup_filterable,
    host_config_get,
    host_run_command,
};

void plugin_fire_tick(Editor *e) {
    for (int i = 0; i < nticks; i++) ticks[i](e);
}

void plugin_fire_save(Editor *e) {
    for (int i = 0; i < nsaves; i++) saves[i](e);
}

const char *plugin_key_command(int key) {
    for (int i = 0; i < npkeys; i++)
        if (pkeys[i].key == key) return pkeys[i].cmd;
    return NULL;
}

#ifndef NNANO_PLUGIN_SYSDIR
#define NNANO_PLUGIN_SYSDIR ""   /* set by the Makefile; "" means no system directory */
#endif

/* Plugins are looked for in $HOME/.nnanoplugins/<name>.so first, then in
 * NNANO_PLUGIN_SYSDIR/<name>.so (a system-wide install). */
void plugins_load(Editor *e) {
    const char *home   = getenv("HOME");
    const char *sysdir = NNANO_PLUGIN_SYSDIR;

    for (int i = 0; i < cfg.nplugins; i++) {
        const char *name = cfg.plugins[i];
        char path[PATH_BUF];

        if (strchr(name, '/')) {
            set_msg(e, "plugin name not allowed: %s", name);
            continue;
        }
        int have_home = home && *home;
        if (have_home)
            snprintf(path, sizeof path, "%s/.nnanoplugins/%s.so", home, name);
        if (!have_home || access(path, R_OK) != 0) {
            if (sysdir[0])
                snprintf(path, sizeof path, "%s/%s.so", sysdir, name);
            else if (!have_home) {
                set_msg(e, "plugin %s: HOME is not set", name);
                continue;
            }
        }

        void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!h) {
            set_msg(e, "plugin %s: %s", name, dlerror());
            continue;
        }
        PluginInit init = (PluginInit)dlsym(h, PLUGIN_ENTRY);
        if (!init) {
            set_msg(e, "plugin %s: no entry point", name);
            dlclose(h);
            continue;
        }
        init(&host_api);
        if (nloaded < MAX_PLUGINS)
            snprintf(loaded[nloaded++], PLUGIN_NAME_LEN, "%s", name);
    }
}

void plugins_list(Editor *e) {
    if (!nloaded) {
        set_msg(e, "No plugins loaded. See cfg");
        return;
    }
    char out[MSG_MAX] = "plugins:";
    for (int i = 0; i < nloaded; i++) {
        size_t used = strlen(out);
        if (sizeof out - used < 2) break;
        snprintf(out + used, sizeof out - used, " %s", loaded[i]);
    }
    set_msg(e, "%s", out);
}
