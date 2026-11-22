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
#include <string.h>
#include "buffer.h"
#include "utf8.h"
#include "util.h"

Line *cur_line(Editor *e) { return &e->buf.lines[e->row]; }

bool has_sel(const Editor *e) {
    return e->marking && (e->row != e->mrow || e->col != e->mcol);
}

void sel_bounds(const Editor *e, Pos *a, Pos *b) {
    Pos p = { e->mrow, e->mcol }, q = { e->row, e->col };
    if (p.r > q.r || (p.r == q.r && p.c > q.c)) { Pos t = p; p = q; q = t; }
    *a = p;
    *b = q;
}

bool in_sel(size_t r, size_t i, Pos a, Pos b) {
    if (r < a.r || r > b.r) return false;
    if (r == a.r && i < a.c) return false;
    if (r == b.r && i >= b.c) return false;
    return true;
}

static void set_clip(Editor *e, char *s, size_t n) {
    free(e->clip);
    e->clip = s;
    e->cliplen = n;
}

static char *range_text(const Editor *e, Pos a, Pos b, size_t *outlen) {
    size_t cap = 64, len = 0;
    char *s = xrealloc(NULL, cap);
    for (size_t r = a.r; r <= b.r; r++) {
        const Line *l = &e->buf.lines[r];
        size_t from = (r == a.r) ? a.c : 0;
        size_t to   = (r == b.r) ? b.c : l->len;
        size_t need = (to - from) + 2;
        while (len + need > cap) { cap *= 2; s = xrealloc(s, cap); }
        memcpy(s + len, l->s + from, to - from);
        len += to - from;
        if (r != b.r) s[len++] = '\n';
    }
    s[len] = '\0';
    *outlen = len;
    return s;
}

static void delete_range(Editor *e, Pos a, Pos b) {
    if (a.r == b.r) {
        line_erase(&e->buf.lines[a.r], a.c, b.c - a.c);
    } else {
        Line *first = &e->buf.lines[a.r];
        Line *last  = &e->buf.lines[b.r];
        line_erase(first, a.c, first->len - a.c);
        line_insert(first, first->len, last->s + b.c, last->len - b.c);
        for (size_t r = a.r + 1; r <= b.r; r++)
            buf_delete_line(&e->buf, a.r + 1);
    }
    e->row = a.r;
    e->col = a.c;
    e->marking = false;
    e->dirty = true;
}

/* Any edit first removes an active selection (returns true if it did).
 * Without a selection it also drops the mark. */
static bool prep_edit(Editor *e) {
    if (has_sel(e)) {
        Pos a, b;
        sel_bounds(e, &a, &b);
        delete_range(e, a, b);
        return true;
    }
    e->marking = false;
    return false;
}


static void newline_raw(Editor *e) {
    Line *cur = cur_line(e);
    Line nl = line_new();
    line_insert(&nl, 0, cur->s + e->col, cur->len - e->col);
    cur->len = e->col;
    cur->s[cur->len] = '\0';
    buf_insert_line(&e->buf, e->row + 1, nl);  /* may move cur; not used after */
    e->row++;
    e->col = 0;
    e->dirty = true;
}

void insert_text(Editor *e, const char *s, size_t n) {
    prep_edit(e);
    if (n == 0) return;

    size_t nl = 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] == '\n') nl++;

    if (nl == 0) {                       /* common case: one line, one memmove */
        line_insert(cur_line(e), e->col, s, n);
        e->col += n;
        e->dirty = true;
        return;
    }

    /* Split the current line at the cursor, make room for all the new rows
     * at once, then fill them. This keeps large pastes linear. */
    Line tail = line_new();
    {
        Line *cur = cur_line(e);
        line_insert(&tail, 0, cur->s + e->col, cur->len - e->col);
        cur->len = e->col;
        cur->s[cur->len] = '\0';
    }
    buf_make_room(&e->buf, e->row + 1, nl);

    size_t rowi = e->row;
    size_t start = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i < n && s[i] != '\n') continue;
        line_insert(&e->buf.lines[rowi], e->buf.lines[rowi].len, s + start, i - start);
        if (i == n) break;
        start = i + 1;
        rowi++;
        e->buf.lines[rowi] = line_new();
    }

    Line *last = &e->buf.lines[rowi];
    e->row = rowi;
    e->col = last->len;
    line_insert(last, last->len, tail.s, tail.len);
    free(tail.s);
    e->dirty = true;
}

void type_codepoint(Editor *e, unsigned int cp) {
    char enc[4];
    size_t n = utf8_encode(cp, enc);
    insert_text(e, enc, n);
}

void newline(Editor *e) {
    prep_edit(e);
    newline_raw(e);
}

void del_back(Editor *e) {
    if (prep_edit(e)) return;
    Line *cur = cur_line(e);
    if (e->col > 0) {
        size_t p = utf8_prev(cur->s, cur->len, e->col);
        line_erase(cur, p, e->col - p);
        e->col = p;
    } else if (e->row > 0) {
        Line *prev = &e->buf.lines[e->row - 1];
        size_t joinat = prev->len;
        line_insert(prev, prev->len, cur->s, cur->len);
        buf_delete_line(&e->buf, e->row);
        e->row--;
        e->col = joinat;
    } else {
        return;
    }
    e->dirty = true;
}

void del_forward(Editor *e) {
    if (prep_edit(e)) return;
    Line *cur = cur_line(e);
    if (e->col < cur->len) {
        size_t nx = utf8_next(cur->s, cur->len, e->col);
        line_erase(cur, e->col, nx - e->col);
    } else if (e->row + 1 < e->buf.n) {
        Line *nxt = &e->buf.lines[e->row + 1];
        line_insert(cur, cur->len, nxt->s, nxt->len);
        buf_delete_line(&e->buf, e->row + 1);
    } else {
        return;
    }
    e->dirty = true;
}

void cut_sel(Editor *e) {
    Pos a, b;
    sel_bounds(e, &a, &b);
    size_t n;
    char *s = range_text(e, a, b, &n);
    set_clip(e, s, n);
    delete_range(e, a, b);
    set_msg(e, "Cut %zu bytes", n);
}

/* Removes the current row, or empties it if it is the only one. */
static void remove_row(Editor *e) {
    if (e->buf.n > 1) {
        buf_delete_line(&e->buf, e->row);
        if (e->row >= e->buf.n) e->row = e->buf.n - 1;
    } else {
        cur_line(e)->len = 0;
        cur_line(e)->s[0] = '\0';
    }
    e->col = 0;
    e->dirty = true;
}

/* Without a selection, cut the whole current line (nano-style). */
void cut_line(Editor *e) {
    prep_edit(e);
    Line *cur = cur_line(e);
    char *s = xrealloc(NULL, cur->len + 2);
    memcpy(s, cur->s, cur->len);
    s[cur->len] = '\n';
    s[cur->len + 1] = '\0';
    set_clip(e, s, cur->len + 1);
    remove_row(e);
    set_msg(e, "Cut line");
}

void delete_line(Editor *e) {
    prep_edit(e);
    remove_row(e);
}

void copy_sel(Editor *e) {
    if (!has_sel(e)) {
        set_msg(e, "No selection (ctrl+t to mark)");
        return;
    }
    Pos a, b;
    sel_bounds(e, &a, &b);
    size_t n;
    char *s = range_text(e, a, b, &n);
    set_clip(e, s, n);
    e->marking = false;
    set_msg(e, "Copied %zu bytes", n);
}

void paste(Editor *e) {
    if (!e->clip) {
        set_msg(e, "Nothing to paste");
        return;
    }
    insert_text(e, e->clip, e->cliplen);
}

void toggle_mark(Editor *e) {
    if (e->marking) {
        e->marking = false;
        set_msg(e, "Mark cleared");
    } else {
        e->marking = true;
        e->mrow = e->row;
        e->mcol = e->col;
        set_msg(e, "Mark set");
    }
}

void clear_mark(Editor *e) { e->marking = false; }

/* Copies the current line below itself and moves the cursor down. */
void duplicate_line(Editor *e) {
    prep_edit(e);
    Line src = *cur_line(e);
    Line dup = line_new();
    line_insert(&dup, 0, src.s, src.len);
    buf_insert_line(&e->buf, e->row + 1, dup);
    e->row++;
    clamp_col(e);
    e->dirty = true;
}

/* Indents the selected lines, or inserts indentation at the cursor.
 * With use_spaces, indentation goes to the next tab stop. */
void indent_lines(Editor *e) {
    char ind[32];

    if (has_sel(e)) {
        Pos a, b;
        sel_bounds(e, &a, &b);
        size_t w = 1;
        if (cfg.use_spaces) { w = (size_t)TAB_WIDTH; memset(ind, ' ', w); }
        else                { ind[0] = '\t'; }
        for (size_t r = a.r; r <= b.r; r++)
            line_insert(&e->buf.lines[r], 0, ind, w);
        if (e->row >= a.r && e->row <= b.r) e->col += w;
        e->marking = false;
        e->dirty = true;
        clamp_col(e);
        return;
    }

    prep_edit(e);
    Line *l = cur_line(e);
    size_t w = 1;
    if (cfg.use_spaces) {
        int col = disp_col(l->s, l->len, e->col);
        w = (size_t)(TAB_WIDTH - col % TAB_WIDTH);
        memset(ind, ' ', w);
    } else {
        ind[0] = '\t';
    }
    line_insert(l, e->col, ind, w);
    e->col += w;
    e->dirty = true;
}

/* Removes one tab, or up to one tab width of spaces, from the start of
 * the current line or of each selected line. */
void unindent(Editor *e) {
    size_t r0 = e->row, r1 = e->row;
    if (has_sel(e)) {
        Pos a, b;
        sel_bounds(e, &a, &b);
        r0 = a.r;
        r1 = b.r;
    }
    for (size_t r = r0; r <= r1; r++) {
        Line *l = &e->buf.lines[r];
        size_t k = 0;
        if (l->len && l->s[0] == '\t') {
            k = 1;
        } else {
            while (k < l->len && k < (size_t)TAB_WIDTH && l->s[k] == ' ') k++;
        }
        if (k) {
            line_erase(l, 0, k);
            e->dirty = true;
            if (r == e->row) e->col = (e->col >= k) ? e->col - k : 0;
        }
    }
    e->marking = false;
    clamp_col(e);
}
