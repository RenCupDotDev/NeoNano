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
#ifndef POPUP_H
#define POPUP_H

#include <stdbool.h>
#include "editor.h"
#include "plugin.h"

/* One popup at a time. Selectable popups call pick(index) on Enter. */
void popup_open(Editor *e, const char *title, const char **lines, int n,
                bool selectable, PopupPick pick, void *ud);
void popup_close(void);
bool popup_active(void);
/* Returns true if the popup used the key. Any other key closes the popup. */
bool popup_key(Editor *e, int key);
/* Makes the open popup re-filter as text is typed (completion). */
void popup_filterable(PopupFilter fn, void *ud);
/* Called by the input code after an edit while a filterable popup is open. */
void popup_after(Editor *e);
/* Draws the popup near the cursor's screen position (cy, cx). */
void popup_draw(int cy, int cx);

#endif
