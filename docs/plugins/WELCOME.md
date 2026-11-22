# Welcome page (welcome plugin)

The welcome plugin shows a start page when neonano opens with no file, similar to the dashboard in LazyVim. It appears only on an empty start, and only once.

## Enable it

```toml
[plugins]
enabled = ["welcome"]
```

Restart neonano. Plugins load at startup.

## What you see

- The neonano logo.
- A list of actions, each with the key that runs it:

| Key | Action | Runs |
|---|---|---|
| n | New file | closes the page, leaves an empty buffer |
| f | Find file (directory tree) | `dirtree` |
| t | Open a terminal | `terminal` |
| l | Start the language server | `lsp start` |
| c | Open the config file | `cfg` |
| h | Command help | `help` |
| q | Quit | `exit` (asks you to save first if needed) |

- A footer with the keys for the page.

## Keys

| Key | Does |
|---|---|
| a letter in brackets | runs that action |
| up, down | move the highlighted action |
| enter | runs the highlighted action |
| esc | closes the page and gives you the editor |

Other letters do nothing on the page.

## Settings

```toml
[welcome]
show = false        # never show the page on an empty start
```

## Showing it again

Type `welcome` in the palette (F1). Type it again to close it.

## How it works

The page is an overlay pane (`PANE_OVERLAY`). It covers the editor area while open, so the editor isn't drawn and gets no keys. Closing the page brings the editor back. The page's actions run through `api->run_command`, so they behave exactly like the palette commands.

The page's text is plain lines. The left margin is fixed, so on very narrow terminals the logo may be cut off.
