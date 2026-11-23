# User guide

## Starting

```
nnano                 # an empty buffer
nnano notes.txt       # open a file (created on first save if it doesn't exist)
```

The header shows the file name and `modified` when there are unsaved changes. The bottom row is the status bar: it shows messages, then the diagnostic on the cursor's line, then a short list of hints.

## Keys

Defaults. Every action can be remapped in `[keys]`.

### Files and exit

| Action | Key | Action name |
|---|---|---|
| Save | Ctrl+s | `save` |
| Exit (press twice if there are unsaved changes) | Ctrl+x | `quit` |
| Command palette | F1 | `palette` |

### Editing

| Action | Key | Action name |
|---|---|---|
| Undo | Ctrl+z | `undo` |
| Redo | Ctrl+y | `redo` |
| Cut (selection, or the whole line) | Ctrl+k | `cut` |
| Copy selection | Alt+c | `copy` |
| Paste | Ctrl+u | `paste` |
| Delete line | Ctrl+d | `delete_line` |
| Duplicate line | Alt+d | `duplicate_line` |
| Indent (selection or cursor) | Tab | `indent` |
| Unindent | Shift+Tab | `unindent` |
| Newline | Enter | `insert_newline` |
| Delete before cursor | Backspace | `delete_back` |
| Delete after cursor | Delete | `delete_forward` |

Undo groups: typing and Enter form one step, and a run of Backspace or Delete forms another. Moving the cursor ends a group.

### Selection

| Action | Key | Action name |
|---|---|---|
| Set or clear the mark (start a selection) | Ctrl+t | `mark` |
| Clear the mark | Esc | `cancel` |
| Select all | Alt+a | `select_all` |

### Moving

| Action | Key | Action name |
|---|---|---|
| Left, right, up, down | arrows | `move_left` and so on |
| Word left, word right | Alt+b, Alt+f | `word_left`, `word_right` |
| Start of line, end of line | Home or Ctrl+a, End or Ctrl+e | `line_start`, `line_end` |
| Start of file, end of file | Alt+<, Alt+> | `buffer_start`, `buffer_end` |
| Page up, page down | PgUp, PgDn | `page_up`, `page_down` |

### Search

| Action | Key | Action name |
|---|---|---|
| Find (prompts) | Ctrl+f | `find` |
| Find next | F3 | `find_next` |
| Go to line (prompts) | Ctrl+g | `goto_line` |

### View

| Action | Key | Action name |
|---|---|---|
| Toggle line numbers | Alt+n | `toggle_line_numbers` |
| Toggle relative line numbers | Alt+l | `toggle_relative` |
| Toggle current-line highlight | Alt+h | `toggle_current_line` |
| Next theme | Alt+t | `cycle_theme` |
| Redraw | Ctrl+l | `redraw` |

### Config and reload

| Action | Key | Action name |
|---|---|---|
| Reload config and themes | Alt+k | `reload_config` |
| Open config.toml | Alt+i | `open_config` |
| Word count | Alt+w | `word_count` |

### Panes (layout)

| Action | Key |
|---|---|
| Move the nearest divider (grow or shrink the focused side) | Ctrl+arrows |
| Move the focused pane next to its neighbour | Ctrl+Shift+arrows |
| Cycle focus between panes | F9 |
| Return focus to the editor from a pane | Esc |
| Close the focused pane, works from the editor too (not the editor itself). No default key: bind it with `close_pane` below | none |

The `close_pane` action works from the editor and from any pane, and it does not replace a plugin's own close key. Bind it in `[keys]`, for example `close_pane = ["ctrl+w"]`, or run `close` from the palette. Plugin panes are still closed by their plugin.

Ctrl+arrows and Ctrl+Shift+arrows only work in terminals that send the standard xterm codes (most do: GNOME Terminal, Konsole, iTerm2, Windows Terminal, xterm). Plain arrows work everywhere.

### Other keys

| Key | Does |
|---|---|
| Alt+w | Count lines, words and bytes (also the `wc` command) |

## The palette (F1)

Press F1, type a command, press Enter. Commands take an optional argument.

| Command | Aliases | What it does |
|---|---|---|
| `write [file]` | `save`, `w` | Save, or save to a file |
| `saveas [file]` | | Save under a new name (prompts if no name) |
| `open <file>` | | Open a file. Refused if there are unsaved changes |
| `open! <file>` | | Open a file, discarding unsaved changes |
| `discard` | | Throw away unsaved changes and reload the saved file (asks first). In a new buffer, clears it |
| `read <file>` | | Insert a file at the cursor |
| `exit` | `quit`, `q` | Quit. Refused if there are unsaved changes |
| `exit!` | | Quit, discarding changes |
| `find [text]` | `whereis` | Search forward |
| `goto <line>` | | Go to a line |
| `cut`, `copy`, `paste`, `mark`, `deleteline` | | Editing actions |
| `undo`, `redo` | | Undo or redo |
| `theme [name]` | | Switch theme (no name: the next one) |
| `reload` | | Reload config.toml and themes (plugins need a restart) |
| `wc` | | Count lines, words, bytes |
| `cfg` | | Open config.toml. `cfg!` discards changes first |
| `plugins` | | List loaded plugins |
| `help [command]` | | List commands, or show one command's usage |

## Configuration

The config file is `~/.config/neonano/config.toml`. If `$XDG_CONFIG_HOME` is set, it's `$XDG_CONFIG_HOME/neonano/config.toml`. neonano creates the file with defaults on first run.

Changes apply after `reload` (Alt+k), or on restart for plugins and key bindings in plugins.

```toml
theme = "neonano"                   # a file in ~/.config/neonano/themes/

[editor]
tab_width = 4                       # width of a tab stop
use_spaces = true                   # Tab inserts spaces to the next tab stop
undo_levels = 500                   # undo steps kept (1 to 2000)
trim_trailing_spaces = false        # remove trailing spaces on save
final_newline = true                # end saved files with a newline

[view]
line_numbers = "left"               # "left", "right", or "none"
relative_numbers = false            # show distance from the cursor
current_line = true                 # highlight the cursor's line
show_header = true                  # the top bar
gutter_separator = "|"              # the character after each line number

[colors]                            # overrides on top of the theme
text = "#ffffff"

[keys]                              # rebind an action: a key, or a list of keys
save = "ctrl+s"
line_start = ["home", "ctrl+a"]
delete_line = ""                    # "" disables the action

[plugins]
enabled = []    # names of files in ~/.nnanoplugins/ (plugins are not included with neonano)

```

Settings that are accepted but not used yet: `scroll_margin`, `autosave_seconds`, `auto_indent`, `line_number_width`, the header format strings. A key the editor doesn't know is ignored without an error.

### Key names

`ctrl+<letter>`, `alt+<char>` (for example `alt+<`), `f1` to `f12`, `tab`, `backtab` (Shift+Tab), `enter`, `esc`, `backspace`, `delete`, `insert`, `home`, `end`, `pageup`, `pagedown`, `up`, `down`, `left`, `right`, and `ctrl+space`.

### Themes

Themes are TOML files in `~/.config/neonano/themes/`. The `neonano` theme is written on first run.

```toml
[colors]
background     = "#2a2e45"
text           = "#9ba2ff"
current_line   = "#363c5a"
gutter         = "#938274"
gutter_current = "#9ba2ff"
header_fg      = "#2a2e45"
header_bg      = "#9ba2ff"
status_fg      = "#2a2e45"
status_bg      = "#00c49a"
error          = "#ed6a5a"
prompt_fg      = "#2a2e45"
prompt_bg      = "#00c49a"
selection_fg   = "#2a2e45"
selection_bg   = "#938274"
```

Colours are `"#rrggbb"` or one of `black red green yellow blue magenta cyan white`. On a terminal that can't redefine colours, each role uses the nearest colour the terminal has.

## Panes

The screen is split into a header, a layout area, and a status bar. The layout area holds the editor, and each enabled plugin can open a pane beside it. Dividers are drawn with lines.

- Focus is shown by the highlighted row in a pane, and by the cursor in the editor.
- The editor only gets keys when it has focus. The header and status bar are always shown.
- Closing a pane (for example with its toggle key) gives its space back to the other panes.

## Messages and diagnostics

The status bar shows, in order: the latest message (for example "Wrote 12 lines"), the diagnostic on the cursor's line if a language server reports one, and then the hints. Messages clear on the next key.

## Files

- Files are read as UTF-8. CRLF line endings become LF on save.
- neonano never saves without you asking.
- Opening a file with unsaved changes is refused. Save first, or use `open!` to discard.
