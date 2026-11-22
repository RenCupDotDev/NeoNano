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
#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

typedef struct {
    char   *s;      /* always NUL-terminated */
    size_t  len;    /* bytes, excluding NUL */
    size_t  cap;
} Line;

typedef struct {
    Line   *lines;
    size_t  n;
    size_t  cap;
} Buffer;

Line line_new(void);
void line_insert(Line *l, size_t at, const char *s, size_t n);
void line_erase(Line *l, size_t at, size_t n);
void buf_insert_line(Buffer *b, size_t at, Line l);
void buf_delete_line(Buffer *b, size_t at);
void buf_free(Buffer *b);
/* Opens `count` empty slots at `at` in one resize and shift. The caller fills them. */
void buf_make_room(Buffer *b, size_t at, size_t count);

#endif
