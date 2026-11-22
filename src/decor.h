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
#ifndef DECOR_H
#define DECOR_H

#include <stddef.h>

/* Severity of a decoration. Lower number is more severe. */
enum { DIAG_ERROR = 1, DIAG_WARN = 2, DIAG_INFO = 3, DIAG_HINT = 4 };

/* Decorations are owned by a name (e.g. "lsp") so a plugin can replace its own. */
void        decor_clear(const char *owner);
void        decor_add(const char *owner, size_t row, size_t c0, size_t c1,
                      int kind, const char *msg);
int         decor_row_kind(size_t row);                 /* worst kind on the row, 0 if none */
int         decor_span_kind(size_t row, size_t col);    /* kind covering byte col, 0 if none */
const char *decor_row_msg(size_t row);                  /* message of the worst item, or NULL */

#endif
