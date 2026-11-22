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
#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_PLUGINS      16
#define PLUGIN_NAME_LEN  64
#define PATH_BUF         4096
#define THEME_NAME_LEN   64
#define MAX_COLOR_KEYS   32
#define COLOR_KEY_LEN    24
#define COLOR_VAL_LEN    32
#define MAX_KEYBINDS     128
#define KEY_NAME_LEN     32
#define MAX_KV           128
#define KV_KEY_LEN       64
#define KV_VAL_LEN       256

typedef struct {
    char key[COLOR_KEY_LEN];
    char value[COLOR_VAL_LEN];
} ColorOverride;

typedef struct {
    char action[KEY_NAME_LEN];
    char key[KEY_NAME_LEN];
} KeyBind;

/* Every key of every section, as "section.key" -> value (strings unquoted). */
typedef struct {
    char key[KV_KEY_LEN];
    char val[KV_VAL_LEN];
} KV;

typedef struct {
    int           tab_width;
    bool          use_spaces;
    int           undo_levels;
    bool          trim_trailing;
    bool          final_newline;
    int           line_numbers;      /* 0 none, 1 left, 2 right */
    bool          relative_numbers;
    bool          current_line;
    bool          show_header;
    char          gutter_sep;
    char          theme[THEME_NAME_LEN];
    int           nplugins;
    char          plugins[MAX_PLUGINS][PLUGIN_NAME_LEN];
    int           ncolors;
    ColorOverride colors[MAX_COLOR_KEYS];
    int           nkeys;
    KeyBind       keys[MAX_KEYBINDS];
    int           nkv;
    KV            kv[MAX_KV];
} Config;

extern Config cfg;

/* Creates the config folder and config.toml if missing, then parses it.
 * Returns an error message, or NULL. */
const char *config_load(void);
/* Restores the built-in defaults before a reload. */
void        config_reset(void);
const char *config_dir(void);
const char *config_file(void);
/* Looks up [section] key from the config file. NULL if not set. */
const char *config_get(const char *section, const char *key);

#endif
