"""The ':' palette. Commands take a name and an optional argument string."""
import subprocess
import tomllib

from theme import list_themes

COMMANDS = {}


def command(*names):
    def register(fn):
        for name in names:
            COMMANDS[name] = fn
        return fn
    return register


def parse_value(raw):
    """Interpret a value the way TOML would, falling back to plain text."""
    try:
        return tomllib.loads("v = " + raw)["v"]
    except tomllib.TOMLDecodeError:
        return raw


@command("save", "w")
def cmd_save(ed, arg):
    ed.save(arg or None)


@command("saveto")
def cmd_saveto(ed, arg):
    if not arg:
        ed.message("saveto: missing path", error=True)
        return
    ed.save(arg)


@command("open", "e")
def cmd_open(ed, arg):
    if not arg:
        ed.message("open: missing path", error=True)
        return
    ed.open_file(arg)


@command("find", "f")
def cmd_find(ed, arg):
    needle = arg or ed.prompt("find: ", ed.last_needle)
    if needle:
        ed.last_needle = needle
        ed.find(needle)


@command("goto", "g")
def cmd_goto(ed, arg):
    if arg.strip().isdigit():
        ed.goto(int(arg))
    else:
        ed.message("goto: give a line number", error=True)


@command("execute", "x")
def cmd_execute(ed, arg):
    cmd = arg or ed.prompt("exec: ")
    if not cmd:
        return
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=10)
    except subprocess.TimeoutExpired:
        ed.message("execute: timed out", error=True)
        return
    output = (res.stdout or res.stderr).rstrip("\n")
    ed.record(None)
    ed.buf.delete_selection()
    ed.buf.insert_text(output)
    ed.message(f"execute: exit {res.returncode}", error=res.returncode != 0)


@command("config", "cfg")
def cmd_config(ed, arg):
    ed.open_file(ed.config_path)


@command("reload")
def cmd_reload(ed, arg):
    ed.reload_config()


@command("theme")
def cmd_theme(ed, arg):
    if not arg:
        ed.message("themes: " + ", ".join(list_themes()))
    else:
        ed.apply_theme(arg)


@command("lines")
def cmd_lines(ed, arg):
    if arg not in ("left", "right", "none"):
        ed.message("usage: lines left|right|none", error=True)
        return
    ed.cfg["view"]["line_numbers"] = arg
    ed.message(f"line numbers: {arg}")


@command("set")
def cmd_set(ed, arg):
    """set section.key value   e.g.  set view.relative_numbers true"""
    key, _, raw = arg.partition(" ")
    section, dot, name = key.partition(".")
    if not dot or not name or not raw.strip():
        ed.message("usage: set section.key value", error=True)
        return
    value = parse_value(raw.strip())
    ed.cfg.setdefault(section, {})[name] = value
    if section == "colors":
        ed.apply_theme(ed.cfg["theme"])
    elif section == "keys":
        ed.rebuild_keys()
    elif section == "editor" and name == "autosave_seconds":
        ed.apply_timeout()
    ed.message(f"{key} = {value!r}")


@command("plugins")
def cmd_plugins(ed, arg):
    loaded = ", ".join(ed.plugins) or "none"
    errors = ("  errors: " + "; ".join(ed.plugin_errors)) if ed.plugin_errors else ""
    ed.message(f"plugins: {loaded}{errors}", error=bool(ed.plugin_errors))


@command("quit", "q")
def cmd_quit(ed, arg):
    ed.request_quit()


@command("quit!", "q!")
def cmd_force_quit(ed, arg):
    ed.should_quit = True


@command("help", "h")
def cmd_help(ed, arg):
    ed.message("commands: " + " ".join(sorted(COMMANDS)))


def run_palette(ed):
    line = ed.prompt(":")
    if not line or not line.strip():
        return
    name, _, arg = line.strip().partition(" ")
    fn = COMMANDS.get(name)
    if fn is None:
        ed.message(f"unknown command: {name}", error=True)
        return
    try:
        fn(ed, arg.strip())
    except Exception as e:  # a bad command must not kill the editor
        ed.message(f"{name}: {e}", error=True)
