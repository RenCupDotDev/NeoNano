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
#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdbool.h>
#include "plugin.h"

/*
 * Tiling layout. The screen is a tree of splits. Its leaves are panes:
 * the editor, plus any panes plugins open (file tree, git, ...).
 * Plugins fill their panes with lines and receive keys while focused.
 *
 * Keys: ctrl+arrows move the nearest divider, ctrl+shift+arrows swap
 * the focused pane with its neighbour, F9 moves focus to the next pane,
 * esc returns focus to the editor.
 */

#define MAX_PANES 16

typedef struct { int x, y, w, h; } Rect;

typedef struct Pane {
    bool     used;
    int      id;
    char     name[32];
    bool     is_editor;
    char   **lines;
    int      nlines;
    int      cursor;        /* selected line */
    int      top;           /* first visible line (view scrolls it) */
    PaneKeyFn on_key;
    void    *ud;
} Pane;

typedef struct { Pane *pane; Rect r; } Placed;
typedef struct { int x, y, len; bool vertical; } Sep;

void  layout_init(void);
Rect  layout_area(void);                    /* the rows and columns below the header */
Pane *layout_focused(void);
Pane *layout_editor_pane(void);
void  layout_focus_editor(void);
bool  layout_is_overlay(const Pane *p);
void  layout_focus_id(int id);              /* 0 means the editor */
void  layout_focus_next(void);
Pane *layout_find(int id);
int   layout_open(const char *name, int side, int percent, PaneKeyFn fn, void *ud);
void  layout_close(int id);
bool  layout_close_focused(void);           /* false if the editor has focus */
void  layout_set_lines(int id, const char **lines, int n);
void  layout_set_cursor(int id, int row);
void  layout_resize(int dir);               /* dir: 0 up, 1 down, 2 right, 3 left */
void  layout_move(Rect area, int dir);
int   layout_place(Rect area, Placed *out, int max);
int   layout_seps(Rect area, Sep *out, int max);

#endif
