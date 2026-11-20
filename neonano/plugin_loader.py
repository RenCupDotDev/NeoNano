"""Plugin loader. A plugin is a folder containing app.py with a setup(api) function,
and optionally a resources/ folder next to it."""
import importlib.util
from pathlib import Path

from actions import action
from commands import command
from config import search_dirs


class PluginAPI:
    def __init__(self, ed, name, folder):
        self._ed = ed
        self.name = name
        self.folder = folder

    def command(self, *names):
        return command(*names)

    def action(self, name):
        return action(name)

    def on(self, event, fn):
        """Events: startup(ed), open(ed, path), save(ed, path), exit(ed)."""
        self._ed.hooks.setdefault(event, []).append(fn)

    def message(self, text, error=False):
        self._ed.message(text, error)

    @property
    def editor(self):
        return self._ed

    @property
    def buffer(self):
        return self._ed.buf

    @property
    def config(self):
        return self._ed.cfg

    def resource(self, *parts):
        return self.folder / "resources" / Path(*parts)


def load_plugins(ed, names):
    loaded, errors = [], []
    for name in names:
        if "/" in name or "\\" in name or name in ("", ".", ".."):
            errors.append(f"{name}: bad plugin name")
            continue
        folder = None
        for base in search_dirs("plugins"):
            if (base / name / "app.py").is_file():
                folder = base / name
                break
        if folder is None:
            errors.append(f"{name}: not found")
            continue
        try:
            spec = importlib.util.spec_from_file_location(
                f"neonano_plugin_{name}", folder / "app.py")
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            module.setup(PluginAPI(ed, name, folder))
            loaded.append(name)
        except Exception as e:
            errors.append(f"{name}: {e}")
    return loaded, errors
