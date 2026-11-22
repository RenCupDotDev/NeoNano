# LSP plugin

The `lsp` plugin connects neonano to a language server: a separate program that understands your code and reports errors, offers completions, and so on. It speaks the Language Server Protocol, so any LSP server works.

Tested with **clangd 18** (C and C++) and **pyright** (Python, installed with `npm install -g pyright`).

## Install a server

| Language | Server | Install |
|---|---|---|
| C, C++ | clangd | `sudo apt install clangd` |
| Python | pyright | `npm install -g pyright` (provides `pyright-langserver`) |
| Rust | rust-analyzer | `rustup component add rust-analyzer` |
| Go | gopls | `go install golang.org/x/tools/gopls@latest` |
| JavaScript, TypeScript | typescript-language-server | `npm install -g typescript typescript-language-server` |
| Shell | bash-language-server | `npm install -g bash-language-server` |

Check that the program is on your `PATH`: `clangd --version` or `pyright-langserver --version`.

## Enable it

```toml
[plugins]
enabled = ["lsp"]

[lsp]
autostart = true
```

Build and install the plugin with `make plugins` and `make install-plugins`, then restart neonano.

## Choose the server

Each language has a built-in default. Change any of them in `[lsp]`:

```toml
[lsp]
autostart = true
c      = "clangd --background-index=false"
python = "pyright-langserver --stdio"
rust   = "rust-analyzer"
go     = "gopls"
```

The name on the left is the language neonano detects from the file extension:

| Language name | Extensions |
|---|---|
| `c` | `.c`, `.h` |
| `cpp` | `.cc`, `.cpp`, `.hpp` |
| `python` | `.py` |
| `rust` | `.rs` |
| `go` | `.go` |
| `javascript` | `.js` |
| `typescript` | `.ts` |
| `shellscript` | `.sh` |

Any other extension is `plaintext`, and no server is started for it (use `lsp start <command>` by hand).

## Start and stop

- **Autostart** (`autostart = true`): when you open or switch to a file of a language with a server, the server starts by itself. Switching to a file of another language stops the current server and starts the right one. If a server fails to start, it is not retried for that language until you restart.
- **By hand:** press F1 and type `lsp start`. This starts the server for the current file. `lsp start pylsp` starts a specific command.
- **Stop:** `lsp stop`.

Status messages tell you what happened: "LSP started", "server did not answer initialize (is it installed?)", "language server exited".

The server is started in the project folder (the folder you launched neonano from), which pyright and clangd use to find configuration.

## Keys

| Key | Command | What it does |
|---|---|---|
| F5 | `lsp hover` | Popup with the type or documentation under the cursor |
| F12 | `lsp def` | Jump to the definition. If it's in another file, that file opens (save first) |
| Ctrl+Space | `lsp complete` | Completion list at the cursor |
| F2 | `lsp rename` | Rename the symbol under the cursor (prompts for the new name) |
| F6 | `lsp actions` | Code actions for this line (quick fixes). Enter applies one |
| F7 | `lsp format` | Format the file |
| F8 | `lsp diag` | Problems list: every error and warning. Enter jumps to one |

## Commands

| Command | What it does |
|---|---|
| `lsp start [cmd]` | Start the server (for this file's language, or `cmd`) |
| `lsp stop` | Stop the server |
| `lsp hover` | Hover information |
| `lsp def` | Go to definition |
| `lsp complete` | Completion |
| `lsp rename [name]` | Rename. Prompts if no name is given |
| `lsp actions` | Code actions |
| `lsp format` | Format the file |
| `lsp diag` | Problems list |
| `lsp autoformat` | Turn format-on-save on or off |

## Diagnostics

Errors and warnings appear as you type. Each one underlines the text it refers to, and puts an `E` (error) or `W` (warning) in the line-number column. Move the cursor onto the line to read the message on the status bar. Lines with no errors show their number as usual.

Diagnostics are sent to the server about 300 ms after you stop typing, and the server replies with new ones. A brief delay after an edit is normal.

## Completion

Completion opens:

- when you press Ctrl+Space, and
- while you type a word, after a short pause.

The list narrows as you type and reopens when you delete characters. It closes when nothing matches or when you type a non-word character.

| Key | Effect |
|---|---|
| Up, Down | move the selection |
| Enter or Tab | insert the selected item |
| Esc | close the list |

Insertion replaces the word you have typed, up to the cursor.

## Signature help

Typing `(` or `,` after a function name shows its parameters in a popup.

## Hover, definition, rename, code actions

- **Hover** shows the type, and documentation if the server has it. Any key closes it.
- **Definition** jumps within the file, or opens the other file. Opening another file replaces the current one, so save first. Otherwise you get "save or discard changes before opening".
- **Rename** changes the symbol in this file only. Renames across files are not supported yet.
- **Code actions** list the fixes the server offers for the errors on this line. For clangd, "Apply fix: change 'totl' to 'total'" is an example. Enter applies it.

## Formatting

`lsp format` (F7) formats the file with the server's formatter. `lsp autoformat` turns on format on save: the file is formatted before each save. Formatting on save waits up to 1.5 seconds for the server, so a slow server delays the save.

## Configuration summary

| Setting | Default | Meaning |
|---|---|---|
| `[lsp] autostart` | `false` | start the server for the open file automatically |
| `[lsp] <language>` | built-in | the command for that language |

Built-in defaults: `c` and `cpp` use `clangd`; `python` uses `pyright-langserver --stdio`; `rust` uses `rust-analyzer`; `go` uses `gopls`; `javascript` and `typescript` use `typescript-language-server --stdio`; `shellscript` uses `bash-language-server start`.

## Limits

- **One server at a time.** Two files of different languages can't be served by two servers at once. The editor switches, so the first file loses its diagnostics until you return to it.
- **One file at a time.** Rename, code actions, and format change only the current file. Edits to other files are ignored, with a message.
- **Columns.** Positions are converted between bytes and UTF-16 units, so accented text and emoji are handled. Servers that report a position inside a multi-byte character land on the nearest earlier character.
- **Full-document updates.** Every change sends the whole file to the server. That's simple and reliable, but slow on very large files.
- **No semantic highlighting, inlay hints, or snippets.**

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| "no language server for this file: set [lsp] in config.toml" | The language has no default and no `[lsp]` entry. Add one, or use `lsp start <command>`. |
| "server did not answer initialize (is it installed?)" | The command isn't on `PATH`. Run it in a terminal to check. |
| Nothing marked after start | The file has no errors, or the server is still indexing. Wait a few seconds, then edit the file. |
| Completion or hover does nothing | Save the file first (the server needs a file name). |
| "save or discard changes before opening …" | A definition is in another file and this one has changes. Save, then press F12 again. |
| Format on save makes saving slow | The server is slow. Turn it off with `lsp autoformat`. |

## Example session (clangd)

Create `demo.c`:

```c
#include <stdio.h>

int main(void) {
    int total = 1;
    printf("%d\n", totl);
    return 0;
}
```

Open it with `nnano demo.c`. With autostart on, the server starts and marks `totl` on line 4 with an `E`. Move the cursor onto the line and press F6, then Enter: the fix replaces `totl` with `total`. Press F8 to see the problems list.
