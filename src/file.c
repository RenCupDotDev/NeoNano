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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "plugin.h"
#include "undo.h"
#include "util.h"

static void parse_into(FILE *f, Buffer *b) {
    Line cur = line_new();
    int ch;
    while ((ch = getc(f)) != EOF) {
        if (ch == '\r') continue;               /* CRLF -> LF */
        if (ch == '\n') {
            buf_insert_line(b, b->n, cur);
            cur = line_new();
        } else {
            char c = (char)ch;
            line_insert(&cur, cur.len, &c, 1);
        }
    }
    if (cur.len > 0 || b->n == 0) buf_insert_line(b, b->n, cur);
    else free(cur.s);
}

bool save_as(Editor *e, const char *path) {
    plugin_fire_save(e);       /* plugins may edit (e.g. format) before the write */
    FILE *f = fopen(path, "wb");
    if (!f) {
        set_msg(e, "Error: %s", strerror(errno));
        return false;
    }
    for (size_t i = 0; i < e->buf.n; i++) {
        Line *l = &e->buf.lines[i];
        size_t len = l->len;
        if (cfg.trim_trailing)
            while (len > 0 && (l->s[len - 1] == ' ' || l->s[len - 1] == '\t')) len--;
        fwrite(l->s, 1, len, f);
        if (i + 1 < e->buf.n || cfg.final_newline) fputc('\n', f);
    }
    if (fclose(f) != 0) {
        set_msg(e, "Error writing file");
        return false;
    }
    if (!e->path || strcmp(e->path, path)) {
        char *np = strdup(path);
        free(e->path);
        e->path = np;
    }
    e->dirty = false;
    set_msg(e, "Wrote %zu lines to %s", e->buf.n, path);
    return true;
}

bool save_file(Editor *e) {
    if (e->path) return save_as(e, e->path);

    char name[256] = "";
    if (!prompt("File Name to Write: ", name, sizeof name) || !name[0]) {
        set_msg(e, "Save cancelled");
        return false;
    }
    return save_as(e, name);
}

bool open_file(Editor *e, const char *path) {
    char *np = strdup(path);
    Buffer nb = {0};

    FILE *f = fopen(path, "rb");
    if (f) {
        parse_into(f, &nb);
        fclose(f);
        set_msg(e, "Read %zu lines", nb.n);
    } else if (errno == ENOENT) {
        buf_insert_line(&nb, 0, line_new());
        set_msg(e, "New file: %s", path);
    } else {
        set_msg(e, "Error: %s", strerror(errno));
        free(np);
        return false;
    }

    buf_free(&e->buf);
    e->buf = nb;
    free(e->path);
    e->path = np;
    e->row = e->col = e->top = 0;
    e->left = 0;
    e->marking = false;
    e->dirty = false;
    undo_clear();
    e->version++;
    return true;
}

bool insert_file(Editor *e, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        set_msg(e, "Error: %s", strerror(errno));
        return false;
    }
    size_t cap = 4096, len = 0;
    char *data = xrealloc(NULL, cap);
    for (;;) {
        if (len == cap) {
            cap *= 2;
            data = xrealloc(data, cap);
        }
        size_t got = fread(data + len, 1, cap - len, f);
        if (got == 0) break;
        len += got;
    }
    fclose(f);

    size_t j = 0;                               /* drop CRs */
    for (size_t i = 0; i < len; i++)
        if (data[i] != '\r') data[j++] = data[i];

    insert_text(e, data, j);
    free(data);
    set_msg(e, "Inserted %s", path);
    return true;
}
