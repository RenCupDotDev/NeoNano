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
#include "layout.h"
#include <stdlib.h>
#include <string.h>
#include "config.h"

#define MAX_NODES 64

typedef struct Node {
    bool         used, leaf, vertical;
    Pane        *pane;
    int          share;            /* percent of the first child */
    struct Node *a, *b;
} Node;

static Pane  panes[MAX_PANES];
static Node  nodes[MAX_NODES];
static Node *root;
static Pane *editor_pane;
static Pane *focus;
static Pane *overlay_pane;   /* a full-screen pane, or NULL */
static int   next_id = 1;

static Pane *pane_alloc(void) {
    for (int i = 0; i < MAX_PANES; i++) {
        if (!panes[i].used) {
            memset(&panes[i], 0, sizeof panes[i]);
            panes[i].used = true;
            panes[i].id = next_id++;
            return &panes[i];
        }
    }
    return NULL;
}

static Node *node_alloc(void) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (!nodes[i].used) {
            memset(&nodes[i], 0, sizeof nodes[i]);
            nodes[i].used = true;
            return &nodes[i];
        }
    }
    return NULL;
}

static void node_free(Node *n) { n->used = false; }

void layout_init(void) {
    memset(panes, 0, sizeof panes);
    memset(nodes, 0, sizeof nodes);
    editor_pane = pane_alloc();
    editor_pane->is_editor = true;
    snprintf(editor_pane->name, sizeof editor_pane->name, "editor");
    Node *leaf = node_alloc();
    leaf->leaf = true;
    leaf->pane = editor_pane;
    root = leaf;
    focus = editor_pane;
}

Rect layout_area(void) {
    int top = cfg.show_header ? 1 : 0;
    Rect r = { 0, top, COLS, LINES - top - 1 };
    if (r.h < 2) r.h = 2;
    return r;
}

Pane *layout_focused(void)     { return focus; }
Pane *layout_editor_pane(void) { return editor_pane; }
bool  layout_is_overlay(const Pane *p) { return overlay_pane && p == overlay_pane; }
void  layout_focus_editor(void){ focus = overlay_pane ? overlay_pane : editor_pane; }

Pane *layout_find(int id) {
    for (int i = 0; i < MAX_PANES; i++)
        if (panes[i].used && panes[i].id == id) return &panes[i];
    return NULL;
}

void layout_focus_id(int id) {
    if (id == 0) { layout_focus_editor(); return; }
    Pane *p = layout_find(id);
    if (p) focus = p;
}

/* Walks the tree and records each leaf's rectangle, and each divider. */
typedef struct { Pane *pane; Rect r; Node *node; } Slot;

static void walk(Node *n, Rect r, Slot *out, int *no, int max,
                 Sep *seps, int *ns, int maxs) {
    if (!n) return;
    if (n->leaf) {
        if (out && *no < max) {
            out[*no].pane = n->pane;
            out[*no].r = r;
            out[*no].node = n;
            (*no)++;
        }
        return;
    }
    if (!n->vertical) {                         /* side by side */
        int aw = r.w * n->share / 100;
        if (aw > r.w - 2) aw = r.w - 2;
        if (aw < 1) aw = 1;
        Rect ra = { r.x, r.y, aw, r.h };
        Rect rb = { r.x + aw + 1, r.y, r.w - aw - 1, r.h };
        if (rb.w < 1) rb.w = 1;
        if (seps && *ns < maxs) seps[(*ns)++] = (Sep){ r.x + aw, r.y, r.h, true };
        walk(n->a, ra, out, no, max, seps, ns, maxs);
        walk(n->b, rb, out, no, max, seps, ns, maxs);
    } else {                                    /* stacked */
        int ah = r.h * n->share / 100;
        if (ah > r.h - 2) ah = r.h - 2;
        if (ah < 1) ah = 1;
        Rect ra = { r.x, r.y, r.w, ah };
        Rect rb = { r.x, r.y + ah + 1, r.w, r.h - ah - 1 };
        if (rb.h < 1) rb.h = 1;
        if (seps && *ns < maxs) seps[(*ns)++] = (Sep){ r.x, r.y + ah, r.w, false };
        walk(n->a, ra, out, no, max, seps, ns, maxs);
        walk(n->b, rb, out, no, max, seps, ns, maxs);
    }
}

int layout_place(Rect area, Placed *out, int max) {
    if (overlay_pane && max > 0) {
        out[0].pane = overlay_pane;
        out[0].r = area;
        return 1;
    }
    Slot tmp[MAX_NODES];
    int no = 0, ns = 0;
    walk(root, area, tmp, &no, MAX_NODES, NULL, &ns, 0);
    int k = no < max ? no : max;
    for (int i = 0; i < k; i++) {
        out[i].pane = tmp[i].pane;
        out[i].r = tmp[i].r;
    }
    return k;
}

int layout_seps(Rect area, Sep *out, int max) {
    if (overlay_pane) return 0;
    Slot tmp[MAX_NODES];
    int no = 0, ns = 0;
    walk(root, area, tmp, &no, MAX_NODES, out, &ns, max);
    return ns;
}

void layout_focus_next(void) {
    if (overlay_pane) { focus = overlay_pane; return; }
    Slot tmp[MAX_NODES];
    int no = 0, ns = 0;
    walk(root, layout_area(), tmp, &no, MAX_NODES, NULL, &ns, 0);
    if (no == 0) return;
    int k = 0;
    for (int i = 0; i < no; i++)
        if (tmp[i].pane == focus) k = i;
    focus = tmp[(k + 1) % no].pane;
}

int layout_open(const char *name, int side, int percent, PaneKeyFn fn, void *ud) {
    if (side == PANE_OVERLAY && overlay_pane) {       /* one overlay_pane at a time */
        layout_close(overlay_pane->id);
    }
    Pane *p = pane_alloc();
    if (!p) return 0;
    snprintf(p->name, sizeof p->name, "%s", name ? name : "pane");
    p->on_key = fn;
    p->ud = ud;
    if (side == PANE_OVERLAY) {
        overlay_pane = p;
        focus = p;
        return p->id;
    }

    Node *leaf = node_alloc();
    Node *split = node_alloc();
    if (!leaf || !split) {
        if (leaf) node_free(leaf);
        if (split) node_free(split);
        p->used = false;
        return 0;
    }
    leaf->leaf = true;
    leaf->pane = p;

    if (percent < 10) percent = 10;
    if (percent > 90) percent = 90;
    split->leaf = false;
    switch (side) {
    case PANE_LEFT:
        split->vertical = false; split->share = percent;
        split->a = leaf; split->b = root;
        break;
    case PANE_RIGHT:
        split->vertical = false; split->share = 100 - percent;
        split->a = root; split->b = leaf;
        break;
    case PANE_TOP:
        split->vertical = true; split->share = percent;
        split->a = leaf; split->b = root;
        break;
    default:
        split->vertical = true; split->share = 100 - percent;
        split->a = root; split->b = leaf;
        break;
    }
    root = split;
    return p->id;
}

static Node *remove_leaf(Node *n, Pane *p) {
    if (!n || n->leaf) return n;
    if (n->a->leaf && n->a->pane == p) {
        Node *keep = n->b;
        node_free(n->a);
        node_free(n);
        return keep;
    }
    if (n->b->leaf && n->b->pane == p) {
        Node *keep = n->a;
        node_free(n->b);
        node_free(n);
        return keep;
    }
    n->a = remove_leaf(n->a, p);
    n->b = remove_leaf(n->b, p);
    return n;
}

bool layout_close_focused(void) {
    Pane *f = layout_focused();
    if (!f || f->is_editor) return false;
    layout_close(f->id);
    return true;
}

void layout_close(int id) {
    Pane *p = layout_find(id);
    if (!p || p->is_editor) return;
    if (p == overlay_pane) {
        overlay_pane = NULL;
    } else {
        root = remove_leaf(root, p);
    }
    if (focus == p) focus = editor_pane;
    for (int i = 0; i < p->nlines; i++) free(p->lines[i]);
    free(p->lines);
    p->lines = NULL;
    p->nlines = 0;
    p->used = false;
}

void layout_set_lines(int id, const char **lines, int n) {
    Pane *p = layout_find(id);
    if (!p || p->is_editor) return;
    for (int i = 0; i < p->nlines; i++) free(p->lines[i]);
    free(p->lines);
    p->lines = NULL;
    p->nlines = 0;
    if (n > 0) {
        p->lines = calloc((size_t)n, sizeof *p->lines);
        if (!p->lines) return;
        for (int i = 0; i < n; i++)
            p->lines[i] = strdup(lines[i] ? lines[i] : "");
        p->nlines = n;
    }
    if (p->cursor >= n) p->cursor = n > 0 ? n - 1 : 0;
}

void layout_set_cursor(int id, int row) {
    Pane *p = layout_find(id);
    if (!p || row < 0) return;
    p->cursor = row < p->nlines ? row : (p->nlines > 0 ? p->nlines - 1 : 0);
}

static bool contains(Node *n, Pane *p) {
    if (!n) return false;
    if (n->leaf) return n->pane == p;
    return contains(n->a, p) || contains(n->b, p);
}

/* The deepest split of the given orientation that holds p. */
static Node *nearest_split(Node *n, Pane *p, bool vertical) {
    if (!n || n->leaf) return NULL;
    Node *child = contains(n->a, p) ? n->a : (contains(n->b, p) ? n->b : NULL);
    if (!child) return NULL;
    Node *inner = nearest_split(child, p, vertical);
    if (inner) return inner;
    return n->vertical == vertical ? n : NULL;
}

void layout_resize(int dir) {
    if (overlay_pane) return;
    bool vertical = dir <= 1;                 /* up/down move a horizontal divider */
    int delta = (dir == 1 || dir == 2) ? 5 : -5;
    Node *s = nearest_split(root, focus, vertical);
    if (!s) return;
    int share = s->share + delta;
    if (share < 10) share = 10;
    if (share > 90) share = 90;
    s->share = share;
}

static bool replace_child(Node *n, Node *old, Node *nu) {
    if (!n || n->leaf) return false;
    if (n->a == old) { n->a = nu; return true; }
    if (n->b == old) { n->b = nu; return true; }
    return replace_child(n->a, old, nu) || replace_child(n->b, old, nu);
}

/* Moves the focused pane next to its neighbour in direction dir. The pane
 * keeps its share of the screen, so a sidebar stays a sidebar. */
void layout_move(Rect area, int dir) {
    if (overlay_pane) return;
    Slot tmp[MAX_NODES];
    int no = 0, ns = 0;
    walk(root, area, tmp, &no, MAX_NODES, NULL, &ns, 0);

    int fi = -1;
    for (int i = 0; i < no; i++)
        if (tmp[i].pane == focus) fi = i;
    if (fi < 0) return;

    int fcx = tmp[fi].r.x + tmp[fi].r.w / 2;
    int fcy = tmp[fi].r.y + tmp[fi].r.h / 2;
    int best = -1;
    long bestscore = 0;
    for (int i = 0; i < no; i++) {
        if (i == fi) continue;
        int cx = tmp[i].r.x + tmp[i].r.w / 2;
        int cy = tmp[i].r.y + tmp[i].r.h / 2;
        bool ok;
        long prim, sec;
        if (dir == 2)      { ok = cx > fcx; prim = cx - fcx; sec = labs((long)cy - fcy); }
        else if (dir == 3) { ok = cx < fcx; prim = fcx - cx; sec = labs((long)cy - fcy); }
        else if (dir == 1) { ok = cy > fcy; prim = cy - fcy; sec = labs((long)cx - fcx); }
        else               { ok = cy < fcy; prim = fcy - cy; sec = labs((long)cx - fcx); }
        if (!ok) continue;
        long score = prim * 4 + sec;
        if (best < 0 || score < bestscore) { best = i; bestscore = score; }
    }
    if (best < 0) return;

    /* The pane's share of the screen, in the direction it is moving. */
    int frac = (dir == 2 || dir == 3)
        ? tmp[fi].r.w * 100 / (area.w > 0 ? area.w : 1)
        : tmp[fi].r.h * 100 / (area.h > 0 ? area.h : 1);
    if (frac < 10) frac = 10;
    if (frac > 90) frac = 90;

    Node *target = tmp[best].node;
    Node *leaf = node_alloc();
    Node *split = node_alloc();
    if (!leaf || !split) {
        if (leaf) node_free(leaf);
        if (split) node_free(split);
        return;
    }
    leaf->leaf = true;
    leaf->pane = focus;

    Pane *p = focus;
    root = remove_leaf(root, p);               /* target node stays in the tree */

    split->leaf = false;
    switch (dir) {
    case 3:  /* left of target */
        split->vertical = false; split->share = frac;
        split->a = leaf; split->b = target; break;
    case 2:  /* right of target */
        split->vertical = false; split->share = 100 - frac;
        split->a = target; split->b = leaf; break;
    case 0:  /* above target */
        split->vertical = true; split->share = frac;
        split->a = leaf; split->b = target; break;
    default: /* below target */
        split->vertical = true; split->share = 100 - frac;
        split->a = target; split->b = leaf; break;
    }

    if (root == target) root = split;
    else replace_child(root, target, split);
    (void)p;
}
