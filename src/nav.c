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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "buffer.h"
#include "utf8.h"
#include "util.h"

/* Keeps the cursor inside the line and on a character boundary. */
void clamp_col(Editor *e) {
    Line *l = cur_line(e);
    if (e->col > l->len) e->col = l->len;
    while (e->col > 0 && e->col < l->len &&
           ((unsigned char)l->s[e->col] & 0xC0) == 0x80)
        e->col--;
}

void move_left(Editor *e) {
    Line *l = cur_line(e);
    if (e->col > 0) {
        e->col = utf8_prev(l->s, l->len, e->col);
    } else if (e->row > 0) {
        e->row--;
        e->col = cur_line(e)->len;
    }
}

void move_right(Editor *e) {
    Line *l = cur_line(e);
    if (e->col < l->len) {
        e->col = utf8_next(l->s, l->len, e->col);
    } else if (e->row + 1 < e->buf.n) {
        e->row++;
        e->col = 0;
    }
}

void move_up(Editor *e) {
    if (e->row > 0) { e->row--; clamp_col(e); }
}

void move_down(Editor *e) {
    if (e->row + 1 < e->buf.n) { e->row++; clamp_col(e); }
}

void line_home(Editor *e) { e->col = 0; }
void line_end(Editor *e)  { e->col = cur_line(e)->len; }

void page_up(Editor *e, size_t page) {
    e->row = (e->row > page) ? e->row - page : 0;
    clamp_col(e);
}

void page_down(Editor *e, size_t page) {
    size_t t = e->row + page;
    e->row = (t >= e->buf.n) ? e->buf.n - 1 : t;
    clamp_col(e);
}

bool find_next(Editor *e, bool skip_current) {
    if (!e->search[0]) return false;
    size_t n = e->buf.n;
    for (size_t i = 0; i <= n; i++) {
        size_t r = (e->row + i) % n;
        const Line *l = &e->buf.lines[r];
        size_t from = 0;
        if (i == 0) from = skip_current ? utf8_next(l->s, l->len, e->col) : e->col;
        if (from > l->len) continue;
        char *p = strstr(l->s + from, e->search);
        if (p) {
            e->row = r;
            e->col = (size_t)(p - l->s);
            return true;
        }
    }
    return false;
}

void goto_line(Editor *e, const char *arg) {
    char *end;
    long v = strtol(arg, &end, 10);
    if (end == arg || v < 1) {
        set_msg(e, "Bad line number");
        return;
    }
    size_t r = (size_t)v - 1;
    if (r >= e->buf.n) r = e->buf.n - 1;
    e->row = r;
    e->col = 0;
}

static bool is_word(unsigned char c) {
    return isalnum(c) || c == '_' || c >= 0x80;   /* UTF-8 bytes count as word */
}

/* Moves to the start of the previous word. */
void word_left(Editor *e) {
    if (e->col == 0) {
        if (e->row > 0) {
            e->row--;
            e->col = cur_line(e)->len;
        }
        return;
    }
    Line *l = cur_line(e);
    size_t c = e->col;
    while (c > 0 && !is_word((unsigned char)l->s[c - 1])) c--;
    while (c > 0 && is_word((unsigned char)l->s[c - 1])) c--;
    e->col = c;
}

/* Moves to the start of the next word. */
void word_right(Editor *e) {
    Line *l = cur_line(e);
    if (e->col >= l->len) {
        if (e->row + 1 < e->buf.n) {
            e->row++;
            e->col = 0;
        }
        return;
    }
    size_t c = e->col;
    while (c < l->len && is_word((unsigned char)l->s[c])) c++;
    while (c < l->len && !is_word((unsigned char)l->s[c])) c++;
    e->col = c;
}

void buffer_start(Editor *e) {
    e->row = 0;
    e->col = 0;
}

void buffer_end(Editor *e) {
    e->row = e->buf.n - 1;
    e->col = cur_line(e)->len;
}

void select_all(Editor *e) {
    buffer_start(e);
    e->marking = true;
    e->mrow = 0;
    e->mcol = 0;
    buffer_end(e);
}
