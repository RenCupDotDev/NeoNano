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
#ifndef EDITOR_H
#define EDITOR_H

#define _XOPEN_SOURCE 700
#define _XOPEN_SOURCE_EXTENDED 1

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>
#include <curses.h>
#include "buffer.h"
#include "config.h"

#define TAB_WIDTH   (cfg.tab_width)
#define GUTTER      6
#define MSG_MAX     256
#define INPUT_NONE  (-2)   /* get_wch error: nothing to do */
#define INPUT_TEXT  (-1)   /* printable text, code point in cp */

typedef struct { size_t r, c; } Pos;

typedef struct Editor {
    Buffer  buf;
    size_t  row, col;       /* cursor: byte offset into the line */
    size_t  top;            /* first visible row */
    int     left;           /* first visible display column */
    char   *path;
    bool    dirty;
    bool    marking;        /* selection active */
    size_t  mrow, mcol;     /* selection anchor */
    char   *clip;
    size_t  cliplen;
    char    search[256];
    char    msg[MSG_MAX];
    bool    quit;
    bool    quit_armed;     /* ctrl+x pressed once on a dirty file */
    unsigned long version;  /* bumped on every edit, for plugins */
} Editor;

/* util.c */
void set_msg(Editor *e, const char *fmt, ...);

/* edit.c */
Line *cur_line(Editor *e);
bool  has_sel(const Editor *e);
void  sel_bounds(const Editor *e, Pos *a, Pos *b);
bool  in_sel(size_t r, size_t i, Pos a, Pos b);
void  insert_text(Editor *e, const char *s, size_t n);
void  type_codepoint(Editor *e, unsigned int cp);
void  newline(Editor *e);
void  del_back(Editor *e);
void  del_forward(Editor *e);
void  cut_sel(Editor *e);
void  cut_line(Editor *e);
void  delete_line(Editor *e);
void  copy_sel(Editor *e);
void  paste(Editor *e);
void  toggle_mark(Editor *e);
void  clear_mark(Editor *e);
void  duplicate_line(Editor *e);
void  indent_lines(Editor *e);
void  unindent(Editor *e);

/* nav.c */
void clamp_col(Editor *e);
void move_left(Editor *e);
void move_right(Editor *e);
void move_up(Editor *e);
void move_down(Editor *e);
void line_home(Editor *e);
void line_end(Editor *e);
void page_up(Editor *e, size_t page);
void page_down(Editor *e, size_t page);
bool find_next(Editor *e, bool skip_current);
void goto_line(Editor *e, const char *arg);
void word_left(Editor *e);
void word_right(Editor *e);
void buffer_start(Editor *e);
void buffer_end(Editor *e);
void select_all(Editor *e);

/* file.c */
bool save_file(Editor *e);                    /* to current path, prompts if none */
bool save_as(Editor *e, const char *path);    /* writes and adopts the path */
bool open_file(Editor *e, const char *path);  /* replaces the buffer */
bool insert_file(Editor *e, const char *path);/* inserts at the cursor */

/* input.c */
bool prompt(const char *label, char *buf, size_t cap);
int  next_key(wint_t *cp);
void input_drain_mode(bool on);
void input_event(Editor *e, int key, unsigned int cp);   /* one key or typed character */   /* non-blocking reads while draining queued input */
void handle_key(Editor *e, int key);

/* view.c */
void draw_screen(Editor *e);

#endif
