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
#ifndef UNDO_H
#define UNDO_H

#include <stdbool.h>
#include "editor.h"

/* Typing a run of characters shares one undo step. */
#define UNDO_NONE 0
#define UNDO_TYPE 1
#define UNDO_DEL  2

/* Records the state before an edit. A UNDO_TYPE checkpoint is skipped
 * when the previous checkpoint was also UNDO_TYPE. */
void undo_checkpoint(Editor *e, int group);
/* Movement ends a typing group. */
void undo_break(void);
/* Forgets all history (used when a file is opened). */
void undo_clear(void);
bool undo_do(Editor *e);
bool redo_do(Editor *e);

#endif
