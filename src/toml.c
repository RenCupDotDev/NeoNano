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
#define _POSIX_C_SOURCE 200809L
#include "toml.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

/* Cuts the line at a # that is not inside quotes. */
static void strip_comment(char *s) {
    bool quoted = false;
    for (char *p = s; *p; p++) {
        if (*p == '"') quoted = !quoted;
        else if (*p == '#' && !quoted) { *p = '\0'; return; }
    }
}

const char *toml_parse_file(const char *path, TomlFn fn, void *ud) {
    static char err[160];
    err[0] = '\0';

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(err, sizeof err, "cannot read %s", path);
        return err;
    }

    char section[64] = "";
    char *line = NULL;
    size_t cap = 0;
    int no = 0;
    ssize_t n;

    while ((n = getline(&line, &cap, f)) != -1) {
        no++;
        strip_comment(line);
        char *p = trim(line);
        if (!*p) continue;

        if (*p == '[') {
            char *end = strchr(p, ']');
            if (!end) {
                snprintf(err, sizeof err, "line %d: bad section header", no);
                break;
            }
            *end = '\0';
            snprintf(section, sizeof section, "%s", trim(p + 1));
            continue;
        }

        char *eq = strchr(p, '=');
        if (!eq) {
            snprintf(err, sizeof err, "line %d: expected key = value", no);
            break;
        }
        *eq = '\0';
        fn(section, trim(p), trim(eq + 1), ud);
    }

    free(line);
    fclose(f);
    return err[0] ? err : NULL;
}

bool toml_string(const char *v, char *out, size_t cap) {
    size_t n = strlen(v);
    if (n < 2 || v[0] != '"' || v[n - 1] != '"') return false;
    snprintf(out, cap, "%.*s", (int)(n - 2), v + 1);
    return true;
}

bool toml_bool(const char *v, bool *out) {
    if (!strcmp(v, "true"))  { *out = true;  return true; }
    if (!strcmp(v, "false")) { *out = false; return true; }
    return false;
}

int toml_array(const char *v, char *items, size_t itemlen, int max) {
    const char *p = v;
    int count = 0;

    while (*p == ' ' || *p == '\t') p++;
    if (*p != '[') return -1;
    p++;

    while (*p && count < max) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == ']' || !*p) break;
        if (*p != '"') { p++; continue; }

        p++;
        const char *s = p;
        while (*p && *p != '"') p++;
        size_t len = (size_t)(p - s);
        if (len >= itemlen) len = itemlen - 1;

        char *dst = items + (size_t)count * itemlen;
        memcpy(dst, s, len);
        dst[len] = '\0';
        count++;

        if (*p == '"') p++;
    }
    return count;
}
