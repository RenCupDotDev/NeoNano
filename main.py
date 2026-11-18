import curses
import sys

import config as cfgmod
from actions import ACTIONS
from buffer import Buffer
from editor import Editor
from plugin_loader import load_plugins
from theme import build_theme

WELCOME = "F1 palette  Ctrl+S save  Ctrl+X quit  Alt+I config"


def main(stdscr, path):
    curses.raw()  # raw, not cbreak: frees Ctrl+S / Ctrl+Q from flow control
    curses.noecho()
    stdscr.keypad(True)
    if hasattr(curses, "set_escdelay"):
        curses.set_escdelay(25)
    curses.start_color()

    cfg, cfg_path, cfg_error = cfgmod.load()
    theme = build_theme(cfg)
    theme.apply()

    buf = Buffer.open(path) if path else Buffer()
    ed = Editor(stdscr, buf, cfg, theme, cfg_path)
    ed.rebuild_keys()
    ed.apply_timeout()

    ed.plugins, ed.plugin_errors = load_plugins(ed, cfg["plugins"]["enabled"])
    ed.emit("startup")

    if cfg_error:
        ed.message(cfg_error, error=True)
    elif ed.plugin_errors:
        ed.message("plugin: " + "; ".join(ed.plugin_errors), error=True)
    else:
        ed.message(WELCOME)

    while not ed.should_quit:
        ed.tick()
        ed.draw()
        name, text = ed.read_key()
        if name is None and text is None:
            continue
        action_name = ed.keymap.get(name) if name else None
        if action_name and action_name in ACTIONS:
            try:
                ACTIONS[action_name](ed)
            except Exception as e:
                ed.message(f"{action_name}: {e}", error=True)
        elif text:
            ed.type_char(text)

    ed.emit("exit")


if __name__ == "__main__":
    file_path = sys.argv[1] if len(sys.argv) > 1 else None
    curses.wrapper(main, file_path)
