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
#include "undo.h"
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "config.h"
#include "util.h"

#define MAX_UNDO 2000

/* A full copy of the buffer and cursor. Simple and fine for typical
 * file sizes; the number of steps is capped by [editor] undo_levels. */
typedef struct {
    char  **lines;
    size_t *lens;
    size_t   n, row, col;
} Snap;

static Snap *undo_st[MAX_UNDO];
static int   nundo;
static Snap *redo_st[MAX_UNDO];
static int   nredo;
static int   last_group;

static Snap *snap_take(const Editor *e) {
    Snap *s = xrealloc(NULL, sizeof *s);
    s->n   = e->buf.n;
    s->row = e->row;
    s->col = e->col;
    s->lines = xrealloc(NULL, s->n * sizeof *s->lines);
    s->lens  = xrealloc(NULL, s->n * sizeof *s->lens);
    for (size_t i = 0; i < s->n; i++) {
        s->lens[i]  = e->buf.lines[i].len;
        s->lines[i] = xrealloc(NULL, s->lens[i] + 1);
        memcpy(s->lines[i], e->buf.lines[i].s, s->lens[i] + 1);
    }
    return s;
}

static void snap_free(Snap *s) {
    for (size_t i = 0; i < s->n; i++) free(s->lines[i]);
    free(s->lines);
    free(s->lens);
    free(s);
}

static void snap_restore(Editor *e, const Snap *s) {
    buf_free(&e->buf);
    for (size_t i = 0; i < s->n; i++) {
        Line l = line_new();
        line_insert(&l, 0, s->lines[i], s->lens[i]);
        buf_insert_line(&e->buf, i, l);
    }
    if (e->buf.n == 0) buf_insert_line(&e->buf, 0, line_new());
    e->row = s->row < e->buf.n ? s->row : e->buf.n - 1;
    e->col = s->col;
    clamp_col(e);
    e->marking = false;
    e->dirty = true;
    e->version++;
}

static int limit(void) {
    int c = cfg.undo_levels;
    if (c < 1) c = 1;
    if (c > MAX_UNDO) c = MAX_UNDO;
    return c;
}

/* Pushes onto a stack, dropping the oldest entry when full. */
static void push(Snap **st, int *n, Snap *s) {
    while (*n >= limit()) {
        snap_free(st[0]);
        memmove(st, st + 1, (size_t)(*n - 1) * sizeof *st);
        (*n)--;
    }
    st[(*n)++] = s;
}

static void clear_stack(Snap **st, int *n) {
    for (int i = 0; i < *n; i++) snap_free(st[i]);
    *n = 0;
}

void undo_checkpoint(Editor *e, int group) {
    e->version++;
    if (group != UNDO_NONE && group == last_group) return;
    clear_stack(redo_st, &nredo);
    push(undo_st, &nundo, snap_take(e));
    last_group = group;
}

void undo_break(void) { last_group = UNDO_NONE; }

void undo_clear(void) {
    clear_stack(undo_st, &nundo);
    clear_stack(redo_st, &nredo);
    last_group = UNDO_NONE;
}

bool undo_do(Editor *e) {
    if (nundo == 0) return false;
    Snap *cur = snap_take(e);
    Snap *s = undo_st[--nundo];
    snap_restore(e, s);
    snap_free(s);
    push(redo_st, &nredo, cur);
    last_group = UNDO_NONE;
    return true;
}

bool redo_do(Editor *e) {
    if (nredo == 0) return false;
    Snap *cur = snap_take(e);
    Snap *s = redo_st[--nredo];
    snap_restore(e, s);
    snap_free(s);
    push(undo_st, &nundo, cur);
    last_group = UNDO_NONE;
    return true;
}
