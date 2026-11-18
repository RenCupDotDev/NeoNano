import copy
import os
import shutil
import sys
import tomllib
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent
DEFAULT_CONFIG = PROJECT_DIR / "config.default.toml"


def user_dir():
    if os.environ.get("NEONANO_HOME"):
        return Path(os.environ["NEONANO_HOME"])
    if sys.platform == "win32":
        return Path(os.environ.get("APPDATA", Path.home())) / "neonano"
    return Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / "neonano"


def search_dirs(name):
    """User folder first, so your own themes and plugins override the bundled ones."""
    return [user_dir() / name, PROJECT_DIR / name]


def _merge(base, extra):
    out = copy.deepcopy(base)
    for key, value in extra.items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key] = _merge(out[key], value)
        else:
            out[key] = value
    return out


def load(path=None):
    """Return (config, config_path, error). Creates the user config on first run."""
    with open(DEFAULT_CONFIG, "rb") as f:
        cfg = tomllib.load(f)
    path = Path(path) if path else user_dir() / "config.toml"
    error = None
    try:
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(DEFAULT_CONFIG, path)
        with open(path, "rb") as f:
            cfg = _merge(cfg, tomllib.load(f))
    except tomllib.TOMLDecodeError as e:
        error = f"config error: {e}"
    except OSError as e:
        error = f"config: {e}"
    return cfg, path, error
