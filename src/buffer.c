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
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "util.h"

Line line_new(void) {
    Line l;
    l.cap = 16;
    l.len = 0;
    l.s = xrealloc(NULL, l.cap);
    l.s[0] = '\0';
    return l;
}

static void line_reserve(Line *l, size_t need) {
    if (need <= l->cap) return;
    size_t nc = l->cap;
    while (nc < need) nc *= 2;
    l->s = xrealloc(l->s, nc);
    l->cap = nc;
}

void line_insert(Line *l, size_t at, const char *s, size_t n) {
    line_reserve(l, l->len + n + 1);
    memmove(l->s + at + n, l->s + at, l->len - at + 1);
    memcpy(l->s + at, s, n);
    l->len += n;
}

void line_erase(Line *l, size_t at, size_t n) {
    memmove(l->s + at, l->s + at + n, l->len - at - n + 1);
    l->len -= n;
}

void buf_insert_line(Buffer *b, size_t at, Line l) {
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 16;
        b->lines = xrealloc(b->lines, b->cap * sizeof *b->lines);
    }
    memmove(&b->lines[at + 1], &b->lines[at], (b->n - at) * sizeof *b->lines);
    b->lines[at] = l;
    b->n++;
}

void buf_delete_line(Buffer *b, size_t at) {
    free(b->lines[at].s);
    memmove(&b->lines[at], &b->lines[at + 1], (b->n - at - 1) * sizeof *b->lines);
    b->n--;
}

void buf_free(Buffer *b) {
    for (size_t i = 0; i < b->n; i++) free(b->lines[i].s);
    free(b->lines);
    b->lines = NULL;
    b->n = b->cap = 0;
}

void buf_make_room(Buffer *b, size_t at, size_t count) {
    if (b->n + count > b->cap) {
        size_t nc = b->cap ? b->cap : 16;
        while (nc < b->n + count) nc *= 2;
        b->lines = xrealloc(b->lines, nc * sizeof *b->lines);
        b->cap = nc;
    }
    memmove(&b->lines[at + count], &b->lines[at], (b->n - at) * sizeof *b->lines);
    b->n += count;
}
