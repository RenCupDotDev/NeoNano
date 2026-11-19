import curses

ESC = "\x1b"

NAMED = {
    curses.KEY_UP: "up",
    curses.KEY_DOWN: "down",
    curses.KEY_LEFT: "left",
    curses.KEY_RIGHT: "right",
    curses.KEY_HOME: "home",
    curses.KEY_END: "end",
    curses.KEY_PPAGE: "pageup",
    curses.KEY_NPAGE: "pagedown",
    curses.KEY_DC: "delete",
    curses.KEY_IC: "insert",
    curses.KEY_BTAB: "backtab",
    curses.KEY_BACKSPACE: "backspace",
    curses.KEY_ENTER: "enter",
}
for _i in range(1, 13):
    NAMED[getattr(curses, f"KEY_F{_i}")] = f"f{_i}"

CHAR_NAMES = {
    "\t": "tab",
    "\n": "enter",
    "\r": "enter",
    "\x1b": "esc",
    "\x7f": "backspace",
    "\x08": "backspace",
    "\x00": "ctrl+space",
}


def key_name(key):
    """Turn a curses key (str or int) into a binding name like 'ctrl+s' or 'f1'."""
    if isinstance(key, int):
        return NAMED.get(key)
    if key in CHAR_NAMES:
        return CHAR_NAMES[key]
    if len(key) == 1 and 1 <= ord(key) <= 26:
        return "ctrl+" + chr(ord(key) + 96)
    return key


def normalize(binding):
    return binding.strip().lower()


def read_key(stdscr, idle_ms=-1):
    """Return (name, text). name is the binding name, text is the typed character if printable.
    Returns (None, None) on timeout or unknown keys."""
    try:
        raw = stdscr.get_wch()
    except curses.error:
        return None, None
    if raw == ESC:
        # Alt+key arrives as ESC followed by the key. Peek without blocking.
        stdscr.timeout(0)
        try:
            nxt = stdscr.get_wch()
        except curses.error:
            nxt = None
        stdscr.timeout(idle_ms)
        if isinstance(nxt, str):
            inner = key_name(nxt)
            if inner:
                return f"alt+{inner}", None
        return "esc", None
    text = raw if isinstance(raw, str) and raw.isprintable() else None
    return key_name(raw), text


def build_keymap(cfg):
    """Map binding name -> action name, from the [keys] table."""
    keymap = {}
    for action_name, binding in cfg.get("keys", {}).items():
        bindings = [binding] if isinstance(binding, str) else list(binding)
        for b in bindings:
            name = normalize(str(b))
            if name:
                keymap[name] = action_name
    return keymap
