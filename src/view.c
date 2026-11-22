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
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "config.h"
#include "decor.h"
#include "layout.h"
#include "popup.h"
#include "theme.h"
#include "utf8.h"

static int digits_of(size_t n) {
    int d = 1;
    while (n >= 10) { n /= 10; d++; }
    return d;
}

/* Width of the line-number column, including the separator and a space. */
static int gutter_width(const Editor *e) {
    if (cfg.line_numbers == 0) return 0;
    return digits_of(e->buf.n) + 2;
}

static void scroll_view(Editor *e, int textrows, int textw) {
    if (e->row < e->top) e->top = e->row;
    if (e->row >= e->top + (size_t)textrows)
        e->top = e->row - (size_t)textrows + 1;

    const Line *l = cur_line(e);
    int dc = disp_col(l->s, l->len, e->col);
    if (dc < e->left) e->left = dc;
    if (dc >= e->left + textw) e->left = dc - textw + 1;
}

/* All drawing below uses absolute screen coordinates. */

static void draw_gutter(const Editor *e, size_t r, int y, int gx, int gw) {
    size_t num;
    if (cfg.relative_numbers && r != e->row)
        num = r > e->row ? r - e->row : e->row - r;
    else
        num = r + 1;

    /* A diagnostic on the line replaces the separator with E or W. */
    int dk = decor_row_kind(r);
    char mark = dk == DIAG_ERROR ? 'E' : dk == DIAG_WARN ? 'W' : cfg.gutter_sep;

    attrset(theme_attr(r == e->row ? PAIR_GUTTER_CUR : PAIR_GUTTER));
    mvprintw(y, gx, "%*zu%c ", gw - 2, num, mark);
}

static void draw_row(const Editor *e, size_t r, int y, int textx, int textw) {
    const Line *l = &e->buf.lines[r];
    bool sel = has_sel(e);
    Pos a = {0, 0}, b = {0, 0};
    if (sel) sel_bounds(e, &a, &b);

    int base = (r == e->row && cfg.current_line) ? PAIR_CURRENT : PAIR_TEXT;
    bool has_decor = decor_row_kind(r) != 0;
    int left = e->left;
    int right = left + textw;
    int dc = 0;
    size_t i = 0;

    attrset(theme_attr(base));
    while (i < l->len && dc < right) {
        unsigned int cp;
        size_t k = utf8_decode(l->s, l->len, i, &cp);
        int w = (cp == '\t') ? TAB_WIDTH - dc % TAB_WIDTH : utf8_cell_width(cp);

        /* Only draw characters that lie fully inside the window. */
        if (w > 0 && dc >= left && dc + w <= right) {
            int x = textx + (dc - left);
            bool hl = sel && in_sel(r, i, a, b);
            int dk = has_decor ? decor_span_kind(r, i) : 0;
            attrset(theme_attr(hl ? PAIR_SELECT : base) | (dk ? A_UNDERLINE : 0));
            if (cp == '\t') {
                for (int j = 0; j < w; j++) mvaddch(y, x + j, ' ');
            } else if (cp < 32 || cp == 127 || cp == 0xFFFD) {
                mvaddch(y, x, '?');
            } else {
                mvaddnstr(y, x, l->s + i, (int)k);
            }
        }
        dc += w;
        i += k;
    }

    int used = dc - left;
    if (used < 0) used = 0;
    if (used > textw) used = textw;
    attrset(theme_attr(base));
    for (int x = used; x < textw; x++) mvaddch(y, textx + x, ' ');
}

/* Draws the editor inside its rectangle. Sets the cursor position if focused. */
static void draw_editor(Editor *e, Rect r, bool focused, int *cy, int *cx) {
    int gw = gutter_width(e);
    int textx = r.x + ((cfg.line_numbers == 2) ? 0 : gw);
    int gx    = (cfg.line_numbers == 2) ? r.x + r.w - gw : r.x;
    int textrows = r.h;
    int textw = r.w - gw;
    if (textrows < 1) textrows = 1;
    if (textw < 1) textw = 1;

    scroll_view(e, textrows, textw);

    for (int y = 0; y < textrows; y++) {
        size_t row = e->top + (size_t)y;
        if (row >= e->buf.n) break;
        if (gw) draw_gutter(e, row, r.y + y, gx, gw);
        draw_row(e, row, r.y + y, textx, textw);
    }

    if (focused) {
        const Line *cl = cur_line(e);
        int dc = disp_col(cl->s, cl->len, e->col);
        int yy = r.y + (int)(e->row - e->top);
        int xx = textx + (dc - e->left);
        if (xx < textx) xx = textx;
        if (xx >= r.x + r.w) xx = r.x + r.w - 1;
        *cy = yy;
        *cx = xx;
    }
}

/* A plugin pane: a title row, then its lines with the cursor line highlighted. */
static void draw_pane(Pane *p, Rect r, bool focused) {
    if (r.h < 2 || r.w < 3) return;

    attrset(theme_attr(PAIR_HEADER));
    for (int x = 0; x < r.w; x++) mvaddch(r.y, r.x + x, ' ');
    mvprintw(r.y, r.x + 1, "%.*s", r.w - 2, p->name);

    int rows = r.h - 1;
    if (p->nlines == 0) p->cursor = 0;
    else if (p->cursor >= p->nlines) p->cursor = p->nlines - 1;
    if (p->cursor < p->top) p->top = p->cursor;
    if (p->cursor >= p->top + rows) p->top = p->cursor - rows + 1;
    if (p->top < 0) p->top = 0;

    for (int i = 0; i < rows; i++) {
        int idx = p->top + i;
        int y = r.y + 1 + i;
        int pair = (idx == p->cursor) ? (focused ? PAIR_SELECT : PAIR_CURRENT) : PAIR_TEXT;
        attrset(theme_attr(pair));
        for (int x = 0; x < r.w; x++) mvaddch(y, r.x + x, ' ');
        if (idx < p->nlines) mvprintw(y, r.x + 1, "%.*s", r.w - 2, p->lines[idx]);
    }
    attrset(A_NORMAL);
}

static void draw_sep(Sep s) {
    attrset(theme_attr(PAIR_GUTTER));
    for (int i = 0; i < s.len; i++) {
        if (s.vertical) mvaddch(s.y + i, s.x, ACS_VLINE);
        else            mvaddch(s.y, s.x + i, ACS_HLINE);
    }
    attrset(A_NORMAL);
}

void draw_screen(Editor *e) {
    Rect area = layout_area();
    erase();

    /* header bar: always one row above the layout */
    if (cfg.show_header) {
        const Line *cl = cur_line(e);
        int dc = disp_col(cl->s, cl->len, e->col);
        char pos[64];
        snprintf(pos, sizeof pos, "Ln %zu, Col %d", e->row + 1, dc + 1);
        attrset(theme_attr(PAIR_HEADER));
        move(0, 0);
        clrtoeol();
        mvprintw(0, 1, "neonano  %s", e->path ? e->path : "[untitled]");
        if (e->dirty) {
            attrset(theme_attr(PAIR_ERROR));
            printw("  modified ");
            attrset(theme_attr(PAIR_HEADER));
        }
        int plen = (int)strlen(pos);
        if (COLS > plen + 1) mvprintw(0, COLS - plen - 1, "%s", pos);
    }

    Placed pl[MAX_PANES];
    int np = layout_place(area, pl, MAX_PANES);
    Sep seps[64];
    int ns = layout_seps(area, seps, 64);
    Pane *f = layout_focused();

    int cy = -1, cx = -1;
    for (int i = 0; i < np; i++) {
        if (pl[i].pane->is_editor) draw_editor(e, pl[i].r, f->is_editor, &cy, &cx);
        else                       draw_pane(pl[i].pane, pl[i].r, pl[i].pane == f);
    }
    for (int i = 0; i < ns; i++) draw_sep(seps[i]);

    /* status bar: a message, else the diagnostic on this line, else hints */
    attrset(theme_attr(PAIR_STATUS));
    move(LINES - 1, 0);
    clrtoeol();
    const char *dm = e->msg[0] ? NULL : decor_row_msg(e->row);
    if (e->msg[0])
        mvprintw(LINES - 1, 0, "%s", e->msg);
    else if (dm)
        mvprintw(LINES - 1, 0, "%s", dm);
    else
        mvprintw(LINES - 1, 0, "F1 :command  ^X exit  ^S save  ^F find  ^T mark  ^K cut  ^C copy  ^U paste  F9 pane");
    attrset(A_NORMAL);

    /* the cursor shows only in the focused editor; popups belong to the editor */
    if (f->is_editor && cy >= 0) {
        popup_draw(cy, cx);
        move(cy, cx);
        curs_set(1);
    } else {
        curs_set(0);
    }
    refresh();
}
