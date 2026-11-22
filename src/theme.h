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
#ifndef THEME_H
#define THEME_H

#include "editor.h"

/* Color roles, as named in [colors] of a theme file. */
enum {
    ROLE_BACKGROUND, ROLE_TEXT, ROLE_CURRENT_LINE,
    ROLE_GUTTER, ROLE_GUTTER_CURRENT,
    ROLE_HEADER_FG, ROLE_HEADER_BG,
    ROLE_STATUS_FG, ROLE_STATUS_BG,
    ROLE_ERROR,
    ROLE_PROMPT_FG, ROLE_PROMPT_BG,
    ROLE_SELECTION_FG, ROLE_SELECTION_BG,
    ROLE_COUNT
};

/* Curses color pairs. */
enum {
    PAIR_TEXT = 1, PAIR_CURRENT, PAIR_GUTTER, PAIR_GUTTER_CUR,
    PAIR_HEADER, PAIR_STATUS, PAIR_PROMPT, PAIR_ERROR, PAIR_SELECT
};

/* Loads the theme named in cfg, applying [colors] overrides. Returns an
 * error message or NULL. Does not touch curses. */
const char *theme_setup(void);
/* Loads a theme by name and sets cfg.theme to it. */
const char *theme_load(const char *name);
/* Loads the next theme file (alphabetical) after cfg.theme. */
const char *theme_cycle(void);
/* Creates or refreshes the color pairs. Call after initscr(), and again
 * after any theme or color change. */
void   theme_init(void);
attr_t theme_attr(int pair);

#endif
