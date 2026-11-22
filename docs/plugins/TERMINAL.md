# Terminal (terminal plugin)

The `terminal` plugin opens a shell in a pane at the bottom of the screen. You can run commands, see their output, and go back to the editor at any time.

## Enable and open

```toml
[plugins]
enabled = ["terminal"]
```

Press **F4**, or type `terminal` in the palette. Press F4 again, or type `terminal`, to close it. Closing stops the shell.

The shell is `$SHELL`, or `/bin/sh` if that is not set. It runs interactively, as in a normal terminal.

## Using it

When the terminal pane has focus:

| Key | Sent to the shell |
|---|---|
| typed characters | as typed (UTF-8) |
| Enter | carriage return |
| Backspace | delete |
| Tab | tab (completion works as in a normal shell) |
| Up, Down, Left, Right, Home | cursor keys |
| Ctrl+c | interrupt the running program |
| Ctrl+d | end of input (exits most shells) |
| Ctrl+l | clear the screen |

Esc or F9 returns focus to the editor. Your typing then goes to the editor again. Press F9 to move between panes.

The pane shows the last 1000 lines of output. The most recent line is highlighted while the pane has focus.

## What works

Anything that prints text and reads lines: `ls`, `git status`, `make`, `python3`, `grep`, `npm`, `cargo build`, and so on. Output is cleaned up enough to read: colour codes are removed, carriage returns and backspaces are applied, and "erase to end of line" is honoured. The shell's prompt is shown.

## What doesn't

- **Full-screen programs** such as `vim`, `nano`, `top`, `less`, and `htop` depend on cursor movement and screen drawing, which the pane does not do. Use them in a separate terminal.
- **Pane size** is fixed at 100 by 24 characters for the program, and the pane doesn't resize with the window.
- **Scrollback** keeps 1000 lines. Older output is dropped.
- **Ctrl+b** opens the directory tree even when the terminal has focus, so the shell never gets it. Use Ctrl+c and other keys normally.

## When the shell exits

If you type `exit`, or the shell is killed, the pane shows `[shell exited]` and the status bar says "shell exited". Press F4 twice (close, then open) to start a new shell.

## Example

Press F4, then type:

```
ls -la
echo hello
git status
```

Each command and its output appear in the pane. Press Esc to return to your file.
