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
#include "decor.h"
#include <stdio.h>
#include <string.h>

#define MAX_DECOR 4096

typedef struct {
    size_t row, c0, c1;
    int    kind;
    char   owner[24];
    char   msg[200];
} Item;

static Item items[MAX_DECOR];
static int  nitems;

void decor_clear(const char *owner) {
    int j = 0;
    for (int i = 0; i < nitems; i++)
        if (strcmp(items[i].owner, owner) != 0) items[j++] = items[i];
    nitems = j;
}

void decor_add(const char *owner, size_t row, size_t c0, size_t c1,
               int kind, const char *msg) {
    if (nitems >= MAX_DECOR) return;
    Item *it = &items[nitems++];
    it->row = row;
    it->c0 = c0;
    it->c1 = c1 > c0 ? c1 : c0 + 1;     /* always at least one byte wide */
    it->kind = kind;
    snprintf(it->owner, sizeof it->owner, "%s", owner);
    snprintf(it->msg, sizeof it->msg, "%s", msg ? msg : "");
}

int decor_row_kind(size_t row) {
    int best = 0;
    for (int i = 0; i < nitems; i++)
        if (items[i].row == row && (best == 0 || items[i].kind < best))
            best = items[i].kind;
    return best;
}

int decor_span_kind(size_t row, size_t col) {
    int best = 0;
    for (int i = 0; i < nitems; i++)
        if (items[i].row == row && col >= items[i].c0 && col < items[i].c1 &&
            (best == 0 || items[i].kind < best))
            best = items[i].kind;
    return best;
}

const char *decor_row_msg(size_t row) {
    const Item *best = NULL;
    for (int i = 0; i < nitems; i++)
        if (items[i].row == row && (!best || items[i].kind < best->kind))
            best = &items[i];
    return best ? best->msg : NULL;
}
