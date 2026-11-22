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
#ifndef TOML_H
#define TOML_H

#include <stdbool.h>
#include <stddef.h>

/* Minimal TOML reader. Supports [section] headers, key = value lines,
 * # comments, quoted strings, true/false, integers, and single-line
 * arrays of strings. Multi-line values are not supported. */

/* Called once per key = value line. `value` is the raw text with the
 * comment removed, so strings keep their quotes. */
typedef void (*TomlFn)(const char *section, const char *key,
                       const char *value, void *ud);

/* Returns NULL on success, or a static error message. */
const char *toml_parse_file(const char *path, TomlFn fn, void *ud);

bool toml_string(const char *v, char *out, size_t cap);   /* "text" -> text */
bool toml_bool(const char *v, bool *out);
/* Fills items[i * itemlen] with each string. Returns the count, or -1. */
int  toml_array(const char *v, char *items, size_t itemlen, int max);

#endif
