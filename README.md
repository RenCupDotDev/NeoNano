<p align="center">
  <img src="assets/logo.png" alt="neonano logo" width="160">
</p>

<h1 align="center">neonano</h1>

<p align="center"><b>If nano and NeoVim had a baby.</b></p>

neonano is a terminal text editor written in Python with ncurses. You write in it
the way you would in nano: type, arrow around, Ctrl+S to save. When you need more,
press F1 to open a `:` command palette in the style of vi, run commands, change
settings live, and load plugins. Everything is configurable: colors, themes,
keybinds, line numbers, autosave, and more.

---

## Installation

### Linux: curl (recommended)

```
curl -fsSL https://raw.githubusercontent.com/YOUR_GITHUB_USER/neonano/main/install.sh | bash
```

Per-user install with no root, into `~/.local/bin/nnano`:

```
curl -fsSL https://raw.githubusercontent.com/YOUR_GITHUB_USER/neonano/main/install.sh | bash -s -- --user
```

Then start it:

```
nnano [file]
```

Run the install command again to update. Your config is kept.

### Linux: from a local copy

```
bash install.sh               # system-wide if root, otherwise per-user
bash install.sh --user        # ~/.local/share/neonano, ~/.local/bin/nnano
bash install.sh --system      # /usr/local/share/neonano, /usr/local/bin/nnano (uses sudo)
```

### Uninstall

```
curl -fsSL https://raw.githubusercontent.com/YOUR_GITHUB_USER/neonano/main/install.sh | bash -s -- --uninstall
```

Add `--purge` to the end to delete your config too.

### Requirements

Python 3.11 or newer with curses.

- Fedora: `sudo dnf install python3 ncurses`
- Debian/Ubuntu: `sudo apt install python3 libncurses6`

### Windows

Run from the project folder with Python 3.11 or newer:

```
pip install windows-curses
python main.py [file]
```

Windows Terminal gives the best colors and key handling.

---

## Keybinds

These are the defaults. Every one of them can be changed in the `[keys]` table of your config.

### Writing

| Key | Action |
|-----|--------|
| Arrows | Move |
| Home / Ctrl+A | Start of line |
| End / Ctrl+E | End of line |
| Alt+B / Alt+F | Word left / word right |
| Alt+< / Alt+> | Start / end of file |
| PageUp / PageDown | Page up / down |
| Enter | New line (keeps indentation) |
| Backspace | Delete before cursor |
| Delete | Delete under cursor |
| Tab | Indent (spaces by default) |
| Shift+Tab (`backtab`) | Unindent |
| Ctrl+D | Delete line |
| Alt+D | Duplicate line |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+L | Redraw the screen |

### Selection and clipboard

| Key | Action |
|-----|--------|
| Ctrl+T | Set or clear the mark. While a mark is set, moving the cursor selects text. |
| Alt+A | Select all |
| Ctrl+K | Cut selection |
| Alt+C | Copy selection |
| Ctrl+U | Paste |
| Esc | Clear the mark and the message |

Typing while text is selected replaces the selection. Tab over a multi-line selection indents every line in it.

### Files and search

| Key | Action |
|-----|--------|
| Ctrl+S | Save |
| Ctrl+X | Quit (press twice if there are unsaved changes) |
| Ctrl+F | Find |
| F3 | Find next |
| Ctrl+G | Go to line |
| Alt+I | Open config.toml inside the editor |
| Alt+K | Reload config and theme |

### View

| Key | Action |
|-----|--------|
| Alt+N | Cycle line numbers: left, right, none |
| Alt+L | Toggle relative line numbers |
| Alt+H | Toggle current-line highlight |
| Alt+T | Cycle through themes |

### Palette and plugins

| Key | Action |
|-----|--------|
| F1 | Open the `:` command palette |
| Alt+W | Word and line count (wordcount plugin) |
| Alt+S | Insert a snippet (snippets plugin) |

Keys that work differently across terminals: Alt combinations need the terminal to send Esc before the key, which Windows Terminal, GNOME Terminal, and most Linux terminals do. Ctrl+Space and Ctrl+arrow keys are not reliable everywhere, so neonano doesn't use them by default.

---

## Command palette

Press F1 to open the `:` prompt on the bottom line. Type a command and press Enter. Esc cancels.

| Command | Aliases | What it does |
|---------|---------|--------------|
| `save [path]` | `w` | Save to the current file, or to `path` |
| `saveto <path>` | | Save to a new path |
| `open <path>` | `e` | Open a file (refused if there are unsaved changes) |
| `find [text]` | `f` | Find text, or prompt for it |
| `goto <n>` | `g` | Jump to line n |
| `execute [cmd]` | `x` | Run a shell command and insert its output at the cursor |
| `config` | `cfg` | Open config.toml |
| `reload` | | Reload config and theme (plugins need a restart) |
| `theme [name]` | | Switch theme, or list available themes |
| `lines left\|right\|none` | | Set line number position |
| `set section.key value` | | Change any setting live, for example `set view.relative_numbers true` |
| `plugins` | | Show loaded plugins and any load errors |
| `quit` | `q` | Quit (refused if there are unsaved changes) |
| `quit!` | `q!` | Quit without saving |
| `help` | `h` | List commands |

Examples:

```
:goto 120
:theme light
:lines right
:set editor.tab_width 2
:set colors.text "#ffffff"
:execute git status
```

---

## Plugins

A plugin is a folder. The folder contains an `app.py` file that defines `setup(api)`, and it may also contain a `resources/` folder for data files.

```
my-plugin/
├── app.py
└── resources/
    └── data.toml
```

A minimal `app.py`:

```python
def setup(api):
    @api.command("hello")
    def cmd_hello(ed, arg):
        ed.message(f"hello {arg or 'world'}")

    @api.action("say_hello")
    def act_hello(ed):
        ed.message("hello from a keybind")

    def on_save(ed, path):
        ed.message(f"saved {path}")

    api.on("save", on_save)
```

Enable it by folder name in your config:

```toml
[plugins]
enabled = ["wordcount", "snippets", "my-plugin"]
```

Plugins are loaded from two places, and your own folder wins when names clash:

- Linux: `~/.config/neonano/plugins/`
- Windows: `%APPDATA%\neonano\plugins\`
- Bundled with neonano: `plugins/` inside the install

To bind a plugin action to a key, add it to `[keys]`:

```toml
[keys]
say_hello = "alt+g"
```

### Plugin API

| Member | Description |
|--------|-------------|
| `api.command(*names)` | Decorator. Registers a palette command. The function receives `(ed, arg)`. |
| `api.action(name)` | Decorator. Registers an action that keybinds can call. The function receives `(ed)`. |
| `api.on(event, fn)` | Hook an event. `startup(ed)`, `open(ed, path)`, `save(ed, path)`, `exit(ed)`. |
| `api.buffer` | The current buffer: `lines`, `row`, `col`, `path`, `dirty`, and editing methods. |
| `api.editor` | The editor object: `message()`, `prompt()`, `record()`, `find()`, `goto()`, and more. |
| `api.config` | The live config dictionary. |
| `api.resource(*parts)` | Path to a file in the plugin's `resources/` folder. |
| `api.message(text, error=False)` | Show a message in the status line. |

Before editing the buffer from a plugin, call `api.editor.record(None)` so the change can be undone.

### Bundled plugins

- **wordcount**: `:wc` and Alt+W show line, word, and character counts.
- **snippets**: `:snip <name>` and Alt+S insert text from `resources/snippets.toml`. Add your own entries there.

---

## Configuration

Settings live in `config.toml`. On first run it is created here:

- Linux: `~/.config/neonano/config.toml`
- Windows: `%APPDATA%\neonano\config.toml`

Open it inside the editor with Alt+I or `:config`. Apply changes with Alt+K or `:reload`.

The full file has comments for every option. Here is an overview of the syntax:

```toml
# Top-level: the theme name. Matches a file in themes/ (yours or bundled).
theme = "neonano"

[editor]
tab_width = 4
use_spaces = true             # Tab inserts spaces instead of \t
auto_indent = true            # Enter keeps the indentation of the line above
scroll_margin = 2             # lines kept visible above and below the cursor
undo_levels = 500
autosave_seconds = 0          # 0 disables autosave
trim_trailing_spaces = false  # remove trailing spaces on save
final_newline = true          # end saved files with a newline

[view]
line_numbers = "left"         # "left", "right", or "none"
line_number_width = "auto"    # "auto" or a fixed number of digits
relative_numbers = false
gutter_separator = "|"
current_line = true
show_header = true
header_left = " neonano  {name}{modified}{mark}"
header_right = "{row}:{col}  {lines} lines "

[colors]
# Overrides on top of the theme. Any color role works.
text = "#ffffff"
background = "#000000"

[plugins]
enabled = ["wordcount", "snippets"]

[keys]
# action = "binding"   or a list of bindings. Use "" to disable.
save = "ctrl+s"
line_start = ["home", "ctrl+a"]
```

### Key names

Use these names in `[keys]`: `ctrl+<letter>`, `alt+<key>`, `f1` through `f12`, `tab`, `backtab`, `enter`, `esc`, `backspace`, `delete`, `insert`, `home`, `end`, `pageup`, `pagedown`, `up`, `down`, `left`, `right`, or any single character.

### Header fields

`header_left` and `header_right` accept these placeholders: `{name}`, `{modified}` (shows * when there are unsaved changes), `{mark}`, `{row}`, `{col}`, `{lines}`.

### Colors and themes

A theme is a TOML file with a `[colors]` table:

```toml
name = "mytheme"

[colors]
background = "#1e1e2e"
text = "#cdd6f4"
current_line = "#313244"
gutter = "#6c7086"
gutter_current = "#a6e3a1"
header_fg = "#1e1e2e"
header_bg = "#89b4fa"
status_fg = "#a6e3a1"
status_bg = "#1e1e2e"
error = "#f38ba8"
prompt_fg = "#1e1e2e"
prompt_bg = "#a6e3a1"
selection_fg = "#1e1e2e"
selection_bg = "#cba6f7"
```

Colors can be hex (`"#rrggbb"`) or basic names (`black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`). Put your theme in `~/.config/neonano/themes/mytheme.toml` and set `theme = "mytheme"`. Bundled themes are `neonano`, `light`, and `mono`.

---

## Project layout

```
neonano/
├── install.sh          installer (curl | bash)
├── main.py             entry point and main loop
├── config.default.toml documented defaults
├── config.py           config loading and merging
├── keys.py             key names and keymap
├── buffer.py           text, selection, undo snapshots
├── editor.py           drawing, prompt, layout, messages
├── actions.py          built-in keybind actions
├── commands.py         the : palette
├── plugin_loader.py    plugin discovery and API
├── theme.py            themes and color roles
├── themes/             bundled themes
└── plugins/            bundled plugins
```

## License

MIT
