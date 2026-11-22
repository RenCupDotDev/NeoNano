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
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "theme.h"
#include "toml.h"

static const char *role_names[ROLE_COUNT] = {
    "background", "text", "current_line",
    "gutter", "gutter_current",
    "header_fg", "header_bg",
    "status_fg", "status_bg",
    "error",
    "prompt_fg", "prompt_bg",
    "selection_fg", "selection_bg",
};

/* Built-in neonano theme, from the #9ba2ff #00c49a #ed6a5a #938274 #2a2e45
 * palette. current_line is a lighter navy derived from the background. */
static const char *builtin[ROLE_COUNT] = {
    "#2a2e45",  /* background      */
    "#9ba2ff",  /* text            */
    "#363c5a",  /* current_line    */
    "#938274",  /* gutter          */
    "#9ba2ff",  /* gutter_current  */
    "#2a2e45",  /* header_fg       */
    "#9ba2ff",  /* header_bg       */
    "#2a2e45",  /* status_fg       */
    "#00c49a",  /* status_bg       */
    "#ed6a5a",  /* error           */
    "#2a2e45",  /* prompt_fg       */
    "#00c49a",  /* prompt_bg       */
    "#2a2e45",  /* selection_fg    */
    "#938274",  /* selection_bg    */
};

static int rgb[ROLE_COUNT][3];

static const struct { const char *name; int r, g, b; } basic[8] = {
    {"black", 0, 0, 0},        {"red", 205, 0, 0},
    {"green", 0, 205, 0},      {"yellow", 205, 205, 0},
    {"blue", 0, 0, 238},       {"magenta", 205, 0, 205},
    {"cyan", 0, 205, 205},     {"white", 229, 229, 229},
};

static int find_role(const char *name) {
    for (int i = 0; i < ROLE_COUNT; i++)
        if (!strcmp(role_names[i], name)) return i;
    return -1;
}

/* Accepts "#rrggbb" or a basic color name. */
static bool parse_color(const char *s, int out[3]) {
    if (s[0] == '#') {
        if (strlen(s) != 7) return false;
        for (int i = 1; i < 7; i++)
            if (!isxdigit((unsigned char)s[i])) return false;
        unsigned long v = strtoul(s + 1, NULL, 16);
        out[0] = (int)((v >> 16) & 0xff);
        out[1] = (int)((v >> 8) & 0xff);
        out[2] = (int)(v & 0xff);
        return true;
    }
    for (int i = 0; i < 8; i++) {
        if (!strcasecmp(s, basic[i].name)) {
            out[0] = basic[i].r;
            out[1] = basic[i].g;
            out[2] = basic[i].b;
            return true;
        }
    }
    return false;
}

struct LoadCtx { char err[160]; };

static void on_color(const char *sec, const char *key, const char *val, void *ud) {
    struct LoadCtx *c = ud;
    if (strcmp(sec, "colors")) return;

    int role = find_role(key);
    if (role < 0) {
        if (!c->err[0]) snprintf(c->err, sizeof c->err, "unknown color role '%s'", key);
        return;
    }
    char s[32];
    if (!toml_string(val, s, sizeof s) || !parse_color(s, rgb[role])) {
        if (!c->err[0]) snprintf(c->err, sizeof c->err, "bad color for '%s'", key);
    }
}

static void write_default_theme(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs("# neonano theme. Colors are \"#rrggbb\" or one of: black red green\n"
          "# yellow blue magenta cyan white. Use F1 :theme or alt+t to switch.\n"
          "\n[colors]\n", f);
    for (int i = 0; i < ROLE_COUNT; i++)
        fprintf(f, "%-16s = \"%s\"\n", role_names[i], builtin[i]);
    fclose(f);
}

static const char *themes_dir(void) {
    static char dir[PATH_BUF];
    snprintf(dir, sizeof dir, "%s/themes", config_dir());
    return dir;
}

const char *theme_load(const char *name) {
    static char err[200];
    char nm[THEME_NAME_LEN];
    snprintf(nm, sizeof nm, "%s", name);     /* name may point at cfg.theme */
    err[0] = '\0';

    for (int i = 0; i < ROLE_COUNT; i++)
        parse_color(builtin[i], rgb[i]);

    const char *dir = themes_dir();
    if (mkdir(dir, 0755) < 0 && errno != EEXIST)
        return "cannot create themes directory";

    char path[PATH_BUF];
    snprintf(path, sizeof path, "%s/neonano.toml", dir);
    if (access(path, F_OK) != 0) write_default_theme(path);

    snprintf(cfg.theme, sizeof cfg.theme, "%s", nm);

    if (!nm[0] || strchr(nm, '/')) {
        snprintf(err, sizeof err, "invalid theme name");
    } else {
        snprintf(path, sizeof path, "%s/%s.toml", dir, nm);
        if (access(path, F_OK) == 0) {
            struct LoadCtx ctx;
            ctx.err[0] = '\0';
            const char *perr = toml_parse_file(path, on_color, &ctx);
            if (perr)            snprintf(err, sizeof err, "theme: %s", perr);
            else if (ctx.err[0]) snprintf(err, sizeof err, "theme %s: %s", nm, ctx.err);
        } else if (strcmp(nm, "neonano") != 0) {
            snprintf(err, sizeof err, "theme '%s' not found, using built-in", nm);
        }
    }

    for (int i = 0; i < cfg.ncolors; i++) {
        int role = find_role(cfg.colors[i].key);
        if (role < 0) {
            if (!err[0]) snprintf(err, sizeof err, "unknown color role '%s'", cfg.colors[i].key);
        } else if (!parse_color(cfg.colors[i].value, rgb[role])) {
            if (!err[0]) snprintf(err, sizeof err, "bad color '%s' for %s",
                                  cfg.colors[i].value, cfg.colors[i].key);
        }
    }
    return err[0] ? err : NULL;
}

const char *theme_setup(void) {
    return theme_load(cfg.theme);
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(a, b);
}

static int list_themes(char names[][THEME_NAME_LEN], int max) {
    DIR *d = opendir(themes_dir());
    if (!d) return 0;
    int n = 0;
    struct dirent *ent;
    while (n < max && (ent = readdir(d)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 5 && !strcmp(ent->d_name + len - 5, ".toml")) {
            snprintf(names[n], THEME_NAME_LEN, "%.*s", (int)(len - 5), ent->d_name);
            n++;
        }
    }
    closedir(d);
    qsort(names, (size_t)n, THEME_NAME_LEN, cmp_str);
    return n;
}

const char *theme_cycle(void) {
    char names[64][THEME_NAME_LEN];
    int n = list_themes(names, 64);
    if (n == 0) return "no themes in ~/.config/neonano/themes";

    int idx = -1;
    for (int i = 0; i < n; i++)
        if (!strcmp(names[i], cfg.theme)) idx = i;

    char next[THEME_NAME_LEN];
    snprintf(next, sizeof next, "%s", names[(idx + 1) % n]);
    return theme_load(next);
}

static long dist3(const int c[3], int r, int g, int b) {
    long dr = c[0] - r, dg = c[1] - g, db = c[2] - b;
    return dr * dr + dg * dg + db * db;
}

/* Nearest of the eight basic colors. */
static short nearest_basic(const int c[3]) {
    short best = 0;
    long bd = -1;
    for (int i = 0; i < 8; i++) {
        long d = dist3(c, basic[i].r, basic[i].g, basic[i].b);
        if (bd < 0 || d < bd) { bd = d; best = (short)i; }
    }
    return best;
}

/* Nearest entry in the xterm 256-color cube and gray ramp. */
static short nearest_256(const int c[3]) {
    static const int lv[6] = { 0, 95, 135, 175, 215, 255 };
    short best = 0;
    long bd = -1;
    for (int r = 0; r < 6; r++)
        for (int g = 0; g < 6; g++)
            for (int b = 0; b < 6; b++) {
                long d = dist3(c, lv[r], lv[g], lv[b]);
                if (bd < 0 || d < bd) { bd = d; best = (short)(16 + 36 * r + 6 * g + b); }
            }
    for (int i = 0; i < 24; i++) {
        int v = 8 + 10 * i;
        long d = dist3(c, v, v, v);
        if (d < bd) { bd = d; best = (short)(232 + i); }
    }
    return best;
}

void theme_init(void) {
    if (!has_colors()) return;

    static bool started;
    if (!started) { start_color(); started = true; }

    /* Exact colors need a free slot per role. Otherwise use the nearest
     * entry the terminal already has. */
    bool exact = can_change_color() && COLORS >= 16 + ROLE_COUNT;
    short slot[ROLE_COUNT];
    for (int i = 0; i < ROLE_COUNT; i++) {
        if (exact) {
            slot[i] = (short)(16 + i);
            init_color(slot[i],
                       (short)(rgb[i][0] * 1000 / 255),
                       (short)(rgb[i][1] * 1000 / 255),
                       (short)(rgb[i][2] * 1000 / 255));
        } else {
            slot[i] = (COLORS >= 256) ? nearest_256(rgb[i]) : nearest_basic(rgb[i]);
        }
    }

    short bg    = slot[ROLE_BACKGROUND];
    short curbg = cfg.current_line ? slot[ROLE_CURRENT_LINE] : bg;

    init_pair(PAIR_TEXT,       slot[ROLE_TEXT],           bg);
    init_pair(PAIR_CURRENT,    slot[ROLE_TEXT],           curbg);
    init_pair(PAIR_GUTTER,     slot[ROLE_GUTTER],         bg);
    init_pair(PAIR_GUTTER_CUR, slot[ROLE_GUTTER_CURRENT], curbg);
    init_pair(PAIR_HEADER,     slot[ROLE_HEADER_FG],      slot[ROLE_HEADER_BG]);
    init_pair(PAIR_STATUS,     slot[ROLE_STATUS_FG],      slot[ROLE_STATUS_BG]);
    init_pair(PAIR_PROMPT,     slot[ROLE_PROMPT_FG],      slot[ROLE_PROMPT_BG]);
    init_pair(PAIR_ERROR,      slot[ROLE_BACKGROUND],     slot[ROLE_ERROR]);
    init_pair(PAIR_SELECT,     slot[ROLE_SELECTION_FG],   slot[ROLE_SELECTION_BG]);

    bkgd(COLOR_PAIR(PAIR_TEXT));
}

attr_t theme_attr(int pair) {
    if (has_colors()) return COLOR_PAIR(pair);
    /* Monochrome: plain text, everything else reversed. */
    switch (pair) {
    case PAIR_TEXT: case PAIR_CURRENT: case PAIR_GUTTER: case PAIR_GUTTER_CUR:
        return A_NORMAL;
    default:
        return A_REVERSE;
    }
}
