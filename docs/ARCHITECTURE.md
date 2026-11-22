# Architecture

## Layout of the repository

```
neonano/
├── Makefile              builds nnano
├── README.md             short overview
├── docs/                 this documentation
├── src/                  the editor (one .c file per concern)
│   ├── editor.h          the Editor struct and internal prototypes
│   ├── buffer.h/.c       lines of text: storage only, no curses
│   ├── utf8.h/.c         UTF-8 decoding, display widths, cursor steps
│   ├── util.h/.c         xrealloc, set_msg
│   ├── edit.c            inserting, deleting, selections, clipboard
│   ├── nav.c             cursor movement, find, goto, word and buffer jumps
│   ├── undo.h/.c         snapshot-based undo and redo
│   ├── file.c            open, save, read a file into the buffer
│   ├── keys.h/.c         key names, default bindings, [keys] overrides
│   ├── input.c           reading keys, routing them to panes, popups, the editor
│   ├── commands.h/.c     the F1 palette: named commands
│   ├── config.h/.c       config.toml loader and generic section.key lookup
│   ├── toml.h/.c         a small TOML reader (sections, strings, bools, arrays)
│   ├── theme.h/.c        themes: 14 colour roles, built-in neonano theme
│   ├── layout.h/.c       tiling layout: the split tree and pane geometry
│   ├── view.c            drawing everything
│   ├── decor.h/.c        diagnostics: underlines, gutter marks, messages
│   ├── popup.h/.c        popups: hover, completion, lists
│   ├── plugin.h          the plugin API (the only header plugins need)
│   ├── plugin.c          loads plugins and implements the API
│   └── main.c            setup and the main loop
└── docs/
```

## How a key travels through the editor

1. `next_key()` (input.c) reads one event with a 100 ms timeout. It returns a key id, a typed character, or nothing on timeout.
2. The main loop calls `plugin_fire_tick()` on a timeout, so plugins can read servers, shells, and timers.
3. `input_event()` routes the key:
   - Ctrl+arrows resize the nearest divider. Ctrl+shift+arrows move the focused pane. F9 cycles focus.
   - If a plugin pane has focus, the pane's key handler gets the key first. If it doesn't use it, plugin key bindings and the palette, save, and find keys still apply.
   - Otherwise the editor gets the key: an open popup first, then plugin key bindings, then the keymap, then typed text.
4. After a typed character or a backspace, `popup_after()` runs, which lets a completion popup re-filter.
5. `draw_screen()` redraws the layout: header, every pane, the separators, the status bar, and the popup.

## Design decisions

- **One buffer.** The editor holds one buffer. This keeps the core small and the undo model simple. Multi-buffer editing is the largest missing feature.
- **Snapshot undo.** Each undo step stores a full copy of the buffer. It's simple and fast for typical files and is capped by `undo_levels`.
- **Asynchronous plugins.** A plugin never blocks the editor waiting for something. Language server replies, shell output, and file changes are read on the 100 ms tick. The one exception is format-on-save, which waits up to 1.5 seconds.
- **Plugins are shared objects.** They're loaded with `dlopen` from `~/.nnanoplugins/`, or from the system plugin directory if not found there. A plugin never links against the editor. It only calls the function table it's given, so plugins survive editor rebuilds as long as the API version matches.
- **The API only grows.** New fields are appended to `PluginAPI`, and the version number goes up. A plugin built for an older version still works, and it checks the version before using newer functions.
- **Columns are bytes.** The editor counts columns in UTF-8 bytes.
- **Layout is a tree.** Each split node has an orientation and a share. Leaves are panes. Moving a pane detaches it and reinserts it next to its neighbour, keeping its share of the screen.

## Build

`make` builds `nnano` from `src/*.c`. Plugins are built separately, from their own repository (see the plugin template).
Requirements: a C11 compiler and the wide ncurses headers (`libncursesw5-dev` on Debian and Ubuntu).
