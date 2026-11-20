"""NeoNano smoke test. Run from the project folder:

    python smoke_test.py

It checks the parts that don't need a real terminal: Python version, curses,
config, keybinds, the text buffer, and the plugin loader. Each check prints
PASS or FAIL. Exit code is 0 when everything passes.
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

# Use a throwaway config folder so the test never touches your real config.
os.environ["NEONANO_HOME"] = tempfile.mkdtemp(prefix="neonano-test-")

results = []


def check(name, fn):
    try:
        fn()
        results.append(True)
        print(f"PASS  {name}")
    except Exception as e:
        results.append(False)
        print(f"FAIL  {name}: {e}")


def t_python():
    assert sys.version_info >= (3, 11), f"need Python 3.11+, you have {sys.version.split()[0]}"


def t_curses():
    import curses  # noqa: F401  (on Windows this needs: pip install windows-curses)


def t_config():
    import config
    cfg, path, err = config.load()
    assert err is None, err
    assert os.path.isfile(path), "default config was not created"
    assert cfg["theme"], "theme missing"
    assert cfg["editor"]["tab_width"] > 0


def t_keys():
    import config
    import keys
    cfg, _, _ = config.load()
    km = keys.build_keymap(cfg)
    assert km.get(keys.normalize(cfg["keys"]["palette"])) == "palette", "palette key is not bound"
    assert keys.key_name("\x13") == "ctrl+s"
    assert keys.key_name("\x1f") == "ctrl+_"
    assert keys.key_name("\t") == "tab"


def t_buffer():
    from buffer import Buffer
    b = Buffer()
    b.insert("hello")
    b.newline()
    b.insert("world")
    assert b.lines == ["hello", "world"], b.lines
    b.backspace()
    assert b.lines == ["hello", "worl"], b.lines
    b.move_to(0, 5)
    b.delete_range((0, 0), (0, 5))
    assert b.lines[0] == "", b.lines


def t_save_and_open():
    from buffer import Buffer
    path = os.path.join(tempfile.mkdtemp(), "t.txt")
    b = Buffer(["one", "two"])
    b.save(path)
    assert Buffer.open(path).lines == ["one", "two"]


class _FakeEditor:
    """Just enough of the editor for the plugin loader."""
    def __init__(self, cfg):
        self.cfg = cfg
        self.hooks = {}
        self.messages = []
        self.buf = None

    def message(self, text, error=False):
        self.messages.append((text, error))


def t_plugins():
    import config
    from plugin_loader import load_plugins
    cfg, _, _ = config.load()
    ed = _FakeEditor(cfg)
    loaded, errors = load_plugins(ed, ["wordcount", "snippets"])
    assert not errors, errors
    assert loaded == ["wordcount", "snippets"], loaded


def t_actions_registered():
    import actions
    for name in ["save", "quit", "palette", "undo", "insert_newline", "mark"]:
        assert name in actions.ACTIONS, f"missing action: {name}"


for name, fn in [
    ("Python version", t_python),
    ("curses module", t_curses),
    ("config loads and is created", t_config),
    ("palette key is bound", t_keys),
    ("text buffer editing", t_buffer),
    ("save and reopen a file", t_save_and_open),
    ("bundled plugins load", t_plugins),
    ("built-in actions registered", t_actions_registered),
]:
    check(name, fn)

passed = sum(results)
print(f"\n{passed}/{len(results)} checks passed")
if passed == len(results):
    print("NeoNano core works. Next, start it with: python main.py notes.md")
sys.exit(0 if passed == len(results) else 1)
