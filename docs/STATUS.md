# Status

What works, what is partial, and what is missing. "Tested" means it was run in a simulated terminal against the real server or shell and checked on the screen.

## Editor core

| Feature | Status |
|---|---|
| Insert, delete, newline, backspace, delete forward | done, tested |
| UTF-8 text, wide characters, tabs | done, tested (accents and emoji) |
| Selections (mark, select all), cut, copy, paste | done, tested |
| Large paste is linear (batched inserts, one redraw per paste) | done, tested |
| Undo and redo (typing and Enter grouped, deletes grouped) | done |
| Find, find next, go to line | done |
| Line numbers (left or right), relative numbers, current-line highlight | done |
| Header bar, status bar, messages | done |
| Save, save as, open, read (insert a file), open-with-discard | done |
| `discard`: throw away unsaved changes, reload the saved file (asks first) | done, tested |
| Themes (14 colour roles, files in ~/.config/neonano/themes, live switching) | done, tested |
| Config file (TOML subset), generic `section.key` lookup | done, tested |
| Keys remappable in `[keys]`, alt and ctrl and function keys | done, tested |
| Multiple buffers (tabs) | **missing** |
| Wrap long lines | **missing** |
| Search backwards, replace | **missing** |
| Mouse | **missing** |
| System clipboard | **missing** (the clipboard is internal) |
| Autosave, auto-indent, scroll margin, header format strings | **missing** (accepted in config, ignored) |

## Layout and panes

| Feature | Status |
|---|---|
| Tiling split tree, side panes (left, right, top, bottom) | done, tested |
| Ctrl+arrows resize the nearest divider | done, tested |
| Ctrl+shift+arrows move a pane, keeping its size | done, tested |
| F9 cycles focus, esc returns to the editor | done, tested |
| Plugin panes receive keys and text while focused | done, tested |
| Full-screen overlay pane | done, tested |
| Mouse-driven resizing | **missing** |

## Plugin API

| Feature | Status |
|---|---|
| Version 6: commands, messages, cursor, ticks, save hook, key bindings, decorations, popups, edits, open file, prompt, panes, overlay panes, popup filtering, config lookup, run commands | done |
| Plugins written in C only | by design |
| Plugin reload without restarting | **missing** (restart needed) |
