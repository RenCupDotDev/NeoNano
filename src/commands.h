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
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include "editor.h"
#include "plugin.h"

void commands_init(void);
bool cmd_register(const char *name, const char *help, CmdFn fn);
void run_palette(Editor *e);
void run_command_line(Editor *e, const char *text);

/* Actions shared by keys and the palette. */
void reload_all(Editor *e);
void cmd_open_config(Editor *e);
void cmd_word_count(Editor *e);

#endif
