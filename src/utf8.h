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
#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>

/* Decodes the character starting at s[i]. Returns its byte length.
 * Invalid bytes return 1 with *cp = 0xFFFD. */
size_t utf8_decode(const char *s, size_t n, size_t i, unsigned int *cp);

/* Writes the UTF-8 encoding of cp to out (at least 4 bytes). Returns length. */
size_t utf8_encode(unsigned int cp, char *out);

/* Byte offset of the character before col, and of the one after col. */
size_t utf8_prev(const char *s, size_t len, size_t col);
size_t utf8_next(const char *s, size_t len, size_t col);

/* Terminal columns used by one non-tab character. */
int utf8_cell_width(unsigned int cp);

/* Display column reached after the first `upto` bytes of the line. */
int disp_col(const char *s, size_t len, size_t upto);

#endif
