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
#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "toml.h"

#define CFG_DEFAULTS { \
    .tab_width = 4, .use_spaces = true, .undo_levels = 500, \
    .trim_trailing = false, .final_newline = true, \
    .line_numbers = 1, .relative_numbers = false, .current_line = true, \
    .show_header = true, .gutter_sep = '|', .theme = "neonano" }

static const Config defaults = CFG_DEFAULTS;
Config cfg = CFG_DEFAULTS;

void config_reset(void) { cfg = defaults; }

static char root_dir[PATH_BUF];
static char conf_dir[PATH_BUF];
static char conf_file[PATH_BUF];

static const char DEFAULT_CONFIG[] =
    "# neonano configuration. Changes apply after F1 :reload or alt+k, or on restart.\n"
    "# Themes live in ~/.config/neonano/themes/<name>.toml\n"
    "\n"
    "theme = \"neonano\"\n"
    "\n"
    "[editor]\n"
    "tab_width = 4\n"
    "use_spaces = true\n"
    "undo_levels = 500\n"
    "trim_trailing_spaces = false\n"
    "final_newline = true\n"
    "\n"
    "[view]\n"
    "line_numbers = \"left\"        # \"left\", \"right\" or \"none\"\n"
    "relative_numbers = false\n"
    "current_line = true\n"
    "show_header = true\n"
    "gutter_separator = \"|\"\n"
    "\n"
    "[colors]\n"
    "# Overrides on top of the theme, for example:\n"
    "# text = \"#ffffff\"\n"
    "\n"
    "[keys]\n"
    "# Rebind an action to a key or a list of keys. \"\" disables it.\n"
    "# save = \"ctrl+s\"\n"
    "# line_start = [\"home\", \"ctrl+a\"]\n"
    "\n"
    "[plugins]\n"
    "enabled = []    # names, loaded from ~/.nnanoplugins/<name>.so\n";

static void build_paths(void) {
    const char *xdg  = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg) snprintf(root_dir, sizeof root_dir, "%s", xdg);
    else snprintf(root_dir, sizeof root_dir, "%s/.config", home ? home : ".");
    snprintf(conf_dir, sizeof conf_dir, "%s/neonano", root_dir);
    snprintf(conf_file, sizeof conf_file, "%s/config.toml", conf_dir);
}

const char *config_dir(void) {
    if (!conf_dir[0]) build_paths();
    return conf_dir;
}

const char *config_file(void) {
    if (!conf_dir[0]) build_paths();
    return conf_file;
}

static void add_keybind(const char *action, const char *key) {
    if (cfg.nkeys >= MAX_KEYBINDS) return;
    KeyBind *k = &cfg.keys[cfg.nkeys++];
    snprintf(k->action, sizeof k->action, "%s", action);
    snprintf(k->key, sizeof k->key, "%s", key);
}

static void kv_store(const char *sec, const char *key, const char *val) {
    if (cfg.nkv >= MAX_KV) return;
    KV *kv = &cfg.kv[cfg.nkv];
    char s[KV_VAL_LEN];
    if (toml_string(val, s, sizeof s))
        snprintf(kv->val, sizeof kv->val, "%s", s);
    else
        snprintf(kv->val, sizeof kv->val, "%s", val);
    snprintf(kv->key, sizeof kv->key, "%s.%s", sec, key);
    cfg.nkv++;
}

const char *config_get(const char *section, const char *key) {
    char full[KV_KEY_LEN];
    snprintf(full, sizeof full, "%s.%s", section, key);
    for (int i = 0; i < cfg.nkv; i++)
        if (!strcmp(cfg.kv[i].key, full)) return cfg.kv[i].val;
    return NULL;
}

static void on_entry(const char *sec, const char *key, const char *val, void *ud) {
    (void)ud;
    bool b;
    kv_store(sec, key, val);

    if (!strcmp(sec, "") && !strcmp(key, "theme")) {
        char t[THEME_NAME_LEN];
        if (toml_string(val, t, sizeof t))
            snprintf(cfg.theme, sizeof cfg.theme, "%s", t);

    } else if (!strcmp(sec, "editor")) {
        if (!strcmp(key, "tab_width")) {
            long v = strtol(val, NULL, 10);
            if (v >= 1 && v <= 16) cfg.tab_width = (int)v;
        } else if (!strcmp(key, "use_spaces")) {
            if (toml_bool(val, &b)) cfg.use_spaces = b;
        } else if (!strcmp(key, "undo_levels")) {
            long v = strtol(val, NULL, 10);
            if (v >= 1 && v <= 2000) cfg.undo_levels = (int)v;
        } else if (!strcmp(key, "trim_trailing_spaces")) {
            if (toml_bool(val, &b)) cfg.trim_trailing = b;
        } else if (!strcmp(key, "final_newline")) {
            if (toml_bool(val, &b)) cfg.final_newline = b;
        }

    } else if (!strcmp(sec, "view")) {
        if (!strcmp(key, "current_line")) {
            if (toml_bool(val, &b)) cfg.current_line = b;
        } else if (!strcmp(key, "relative_numbers")) {
            if (toml_bool(val, &b)) cfg.relative_numbers = b;
        } else if (!strcmp(key, "show_header")) {
            if (toml_bool(val, &b)) cfg.show_header = b;
        } else if (!strcmp(key, "line_numbers")) {
            char s[16];
            if (toml_string(val, s, sizeof s))
                cfg.line_numbers = !strcmp(s, "none") ? 0 : !strcmp(s, "right") ? 2 : 1;
        } else if (!strcmp(key, "gutter_separator")) {
            char s[8];
            if (toml_string(val, s, sizeof s)) cfg.gutter_sep = s[0] ? s[0] : ' ';
        }

    } else if (!strcmp(sec, "plugins") && !strcmp(key, "enabled")) {
        char tmp[MAX_PLUGINS][PLUGIN_NAME_LEN];
        int n = toml_array(val, &tmp[0][0], PLUGIN_NAME_LEN, MAX_PLUGINS);
        if (n >= 0) {
            cfg.nplugins = n;
            memcpy(cfg.plugins, tmp, sizeof tmp);
        }

    } else if (!strcmp(sec, "keys")) {
        char items[4][KEY_NAME_LEN];
        int n = toml_array(val, &items[0][0], KEY_NAME_LEN, 4);
        if (n < 0) {
            char s[KEY_NAME_LEN];
            if (toml_string(val, s, sizeof s)) add_keybind(key, s);
        } else if (n == 0) {
            add_keybind(key, "");
        } else {
            for (int i = 0; i < n; i++) add_keybind(key, items[i]);
        }

    } else if (!strcmp(sec, "colors") && cfg.ncolors < MAX_COLOR_KEYS) {
        char s[COLOR_VAL_LEN];
        if (toml_string(val, s, sizeof s)) {
            ColorOverride *o = &cfg.colors[cfg.ncolors++];
            snprintf(o->key, sizeof o->key, "%s", key);
            snprintf(o->value, sizeof o->value, "%s", s);
        }
    }
}

const char *config_load(void) {
    build_paths();

    if (mkdir(root_dir, 0755) < 0 && errno != EEXIST)
        return "cannot create config directory";
    if (mkdir(conf_dir, 0755) < 0 && errno != EEXIST)
        return "cannot create neonano config directory";

    FILE *f = fopen(conf_file, "r");
    if (!f) {
        if (errno != ENOENT) return "cannot read config.toml";
        f = fopen(conf_file, "w");
        if (!f) return "cannot create config.toml";
        fputs(DEFAULT_CONFIG, f);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return toml_parse_file(conf_file, on_entry, NULL);
}
