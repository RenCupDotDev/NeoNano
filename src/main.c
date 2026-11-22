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
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "commands.h"
#include "config.h"
#include "keys.h"
#include "layout.h"
#include "plugin.h"
#include "theme.h"
#include "undo.h"

/* NNANO_VERSION comes from the VERSION file, passed in by the Makefile. */
#ifndef NNANO_VERSION
#define NNANO_VERSION "unknown"
#endif

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("nnano %s\n", NNANO_VERSION);
        return 0;
    }
    if (argc > 2) {
        fprintf(stderr, "usage: %s [file]\n", argv[0]);
        return 1;
    }
    setlocale(LC_ALL, "");    /* needed for UTF-8 input and wcwidth */

    const char *cfg_err   = config_load();
    const char *theme_err = theme_setup();
    const char *key_err   = keymap_setup();
    commands_init();
    layout_init();

    Editor ed;
    memset(&ed, 0, sizeof ed);

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    timeout(100);                  /* getch returns after 100 ms so plugins can tick */
    theme_init();

    if (argc == 2) open_file(&ed, argv[1]);
    if (ed.buf.n == 0) buf_insert_line(&ed.buf, 0, line_new());

    const char *err = cfg_err ? cfg_err : theme_err ? theme_err : key_err;
    if (err) set_msg(&ed, "%s", err);

    plugins_load(&ed);
    if (!ed.msg[0])
        set_msg(&ed, "F1 :command  ^X exit  ^S save  ^F find  ^T mark  ^K cut  ^C copy  ^U paste  F9 pane");

    while (!ed.quit) {
        draw_screen(&ed);
        wint_t cp = 0;
        int key = next_key(&cp);
        if (key == INPUT_NONE) {          /* idle: timeout, or a read error */
            plugin_fire_tick(&ed);
            continue;
        }
        input_event(&ed, key, (unsigned int)cp);

        /* Apply everything already queued (a terminal paste arrives as many
         * keys) before the next redraw. */
        input_drain_mode(true);
        while (!ed.quit) {
            key = next_key(&cp);
            if (key == INPUT_NONE) break;
            input_event(&ed, key, (unsigned int)cp);
        }
        input_drain_mode(false);
    }

    endwin();
    buf_free(&ed.buf);
    free(ed.path);
    free(ed.clip);
    return 0;
}
