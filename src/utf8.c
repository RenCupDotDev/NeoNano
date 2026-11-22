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
#include <wchar.h>
#include "utf8.h"

size_t utf8_decode(const char *s, size_t n, size_t i, unsigned int *cp) {
    unsigned char c = (unsigned char)s[i];
    size_t k;
    unsigned int v;

    if (c < 0x80)                      { k = 1; v = c; }
    else if ((c & 0xE0) == 0xC0)       { k = 2; v = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0)       { k = 3; v = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0)       { k = 4; v = c & 0x07; }
    else { *cp = 0xFFFD; return 1; }

    if (i + k > n) { *cp = 0xFFFD; return 1; }
    for (size_t j = 1; j < k; j++) {
        unsigned char cc = (unsigned char)s[i + j];
        if ((cc & 0xC0) != 0x80) { *cp = 0xFFFD; return 1; }
        v = (v << 6) | (cc & 0x3F);
    }
    *cp = v;
    return k;
}

size_t utf8_encode(unsigned int cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

size_t utf8_prev(const char *s, size_t len, size_t col) {
    if (col == 0) return 0;
    if (col > len) col = len;
    size_t i = col - 1;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
    return i;
}

size_t utf8_next(const char *s, size_t len, size_t col) {
    if (col >= len) return len;
    size_t i = col + 1;
    while (i < len && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
    return i;
}

int utf8_cell_width(unsigned int cp) {
    if (cp < 32 || cp == 127 || cp == 0xFFFD) return 1;  /* drawn as '?' */
    int w = wcwidth((wchar_t)cp);
    return w < 0 ? 1 : w;
}

int disp_col(const char *s, size_t len, size_t upto) {
    int dc = 0;
    size_t i = 0;
    while (i < upto && i < len) {
        unsigned int cp;
        size_t k = utf8_decode(s, len, i, &cp);
        if (cp == '\t') dc += TAB_WIDTH - dc % TAB_WIDTH;
        else            dc += utf8_cell_width(cp);
        i += k;
    }
    return dc;
}
