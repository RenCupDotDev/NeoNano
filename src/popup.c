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
#include "popup.h"
#include <stdio.h>
#include <string.h>
#include "theme.h"

#define MAX_ITEMS 64
#define ITEM_LEN  200
#define MAX_VIS   12
#define MAX_W     70

static bool        active;
static bool        selectable;
static char        title[96];
static char        items[MAX_ITEMS][ITEM_LEN];
static int         nitems, sel, top;
static PopupPick   pick_fn;
static void       *pick_ud;
static PopupFilter filter_fn;
static void       *filter_ud;

void popup_open(Editor *e, const char *t, const char **lines, int n,
                bool sel_able, PopupPick pick, void *ud) {
    (void)e;
    if (n > MAX_ITEMS) n = MAX_ITEMS;
    snprintf(title, sizeof title, "%s", t ? t : "");
    for (int i = 0; i < n; i++)
        snprintf(items[i], ITEM_LEN, "%s", lines[i] ? lines[i] : "");
    nitems = n;
    sel = top = 0;
    selectable = sel_able;
    pick_fn = pick;
    pick_ud = ud;
    active = n > 0;
    filter_fn = NULL;
    filter_ud = NULL;
}

void popup_filterable(PopupFilter fn, void *ud) {
    filter_fn = fn;
    filter_ud = ud;
}

void popup_after(Editor *e) {
    if (!active || !filter_fn) return;
    PopupFilter f = filter_fn;
    void *u = filter_ud;
    f(e, u);
}

void popup_close(void) {
    active = false;
    pick_fn = NULL;
}

bool popup_active(void) { return active; }

static void keep_visible(void) {
    if (sel < top) top = sel;
    if (sel >= top + MAX_VIS) top = sel - MAX_VIS + 1;
}

bool popup_key(Editor *e, int key) {
    if (!active) return false;
    if (selectable) {
        if (key == KEY_UP)   { if (sel > 0) sel--; keep_visible(); return true; }
        if (key == KEY_DOWN) { if (sel + 1 < nitems) sel++; keep_visible(); return true; }
        if (key == 10 || key == 9) {          /* enter or tab: pick */
            PopupPick f = pick_fn;
            void *u = pick_ud;
            int idx = sel;
            popup_close();                    /* close first: pick may open another popup */
            if (f) f(e, idx, u);
            return true;
        }
        if (key == 27) { popup_close(); return true; }
    }
    /* A filterable popup stays open while text is typed or deleted. */
    if (filter_fn && (key == INPUT_TEXT || key == 127 || key == 8)) return false;
    popup_close();
    return false;
}

void popup_draw(int cy, int cx) {
    if (!active) return;

    int vis = nitems < MAX_VIS ? nitems : MAX_VIS;
    int w = (int)strlen(title) + 4;
    for (int i = 0; i < vis; i++) {
        int l = (int)strlen(items[top + i]) + 2;
        if (l > w) w = l;
    }
    if (w > MAX_W) w = MAX_W;
    if (w > COLS) w = COLS;
    if (w < 12) w = 12;
    if (w > COLS) w = COLS;

    int h = vis + 1;
    int y = cy + 1;
    if (y + h > LINES - 1) y = cy - h;          /* flip above the cursor */
    if (y < 1) y = 1;
    int x = cx;
    if (x + w > COLS) x = COLS - w;
    if (x < 0) x = 0;

    attrset(theme_attr(PAIR_HEADER));
    for (int k = 0; k < w; k++) mvaddch(y, x + k, ' ');
    mvprintw(y, x + 1, "%.*s", w - 2, title);

    for (int i = 0; i < vis; i++) {
        int idx = top + i;
        int row = y + 1 + i;
        bool on = selectable && idx == sel;
        attrset(theme_attr(on ? PAIR_SELECT : PAIR_PROMPT));
        for (int k = 0; k < w; k++) mvaddch(row, x + k, ' ');
        mvprintw(row, x + 1, "%.*s", w - 2, items[idx]);
    }
    attrset(A_NORMAL);
}
