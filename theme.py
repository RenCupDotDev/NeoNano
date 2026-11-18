import curses
import tomllib

from config import search_dirs

DEFAULTS = {
    "background": "#2a2e45",
    "text": "#9ba2ff",
    "current_line": "#363b5a",
    "gutter": "#938274",
    "gutter_current": "#00c49a",
    "header_fg": "#2a2e45",
    "header_bg": "#938274",
    "status_fg": "#00c49a",
    "status_bg": "#2a2e45",
    "error": "#ed6a5a",
    "prompt_fg": "#2a2e45",
    "prompt_bg": "#00c49a",
    "selection_fg": "#2a2e45",
    "selection_bg": "#9ba2ff",
}

# attribute role -> (foreground color role, background color role)
ROLE_PAIRS = {
    "text": ("text", "background"),
    "current_line": ("text", "current_line"),
    "gutter": ("gutter", "background"),
    "gutter_current": ("gutter_current", "current_line"),
    "header": ("header_fg", "header_bg"),
    "status": ("status_fg", "status_bg"),
    "error": ("error", "status_bg"),
    "prompt": ("prompt_fg", "prompt_bg"),
    "selection": ("selection_fg", "selection_bg"),
}

BASIC = {
    "black": curses.COLOR_BLACK,
    "red": curses.COLOR_RED,
    "green": curses.COLOR_GREEN,
    "yellow": curses.COLOR_YELLOW,
    "blue": curses.COLOR_BLUE,
    "magenta": curses.COLOR_MAGENTA,
    "cyan": curses.COLOR_CYAN,
    "white": curses.COLOR_WHITE,
}

BASIC_RGB = {
    curses.COLOR_BLACK: (0, 0, 0),
    curses.COLOR_RED: (205, 0, 0),
    curses.COLOR_GREEN: (0, 205, 0),
    curses.COLOR_YELLOW: (205, 205, 0),
    curses.COLOR_BLUE: (0, 0, 238),
    curses.COLOR_MAGENTA: (205, 0, 205),
    curses.COLOR_CYAN: (0, 205, 205),
    curses.COLOR_WHITE: (229, 229, 229),
}


def parse_color(value):
    """'#rrggbb' -> (r, g, b); a basic name -> curses color constant."""
    value = str(value).strip().lower()
    if value.startswith("#") and len(value) == 7:
        return tuple(int(value[i:i + 2], 16) for i in (1, 3, 5))
    if value in BASIC:
        return BASIC[value]
    raise ValueError(f"bad color: {value}")


def _nearest_basic(rgb):
    return min(BASIC_RGB, key=lambda c: sum((a - b) ** 2 for a, b in zip(BASIC_RGB[c], rgb)))


def list_themes():
    names = set()
    for folder in search_dirs("themes"):
        if folder.is_dir():
            names.update(p.stem for p in folder.glob("*.toml"))
    return sorted(names)


def build_theme(cfg):
    name = cfg["theme"]
    colors = None
    for folder in search_dirs("themes"):
        path = folder / f"{name}.toml"
        if path.is_file():
            with open(path, "rb") as f:
                colors = dict(tomllib.load(f).get("colors", {}))
            break
    if colors is None:
        raise FileNotFoundError(name)
    colors.update(cfg.get("colors", {}))
    return Theme(colors)


class Theme:
    def __init__(self, colors):
        self.colors = colors
        self._attrs = {}

    def apply(self):
        """Call after curses.start_color()."""
        values = {}
        for role in DEFAULTS:
            raw = self.colors.get(role, DEFAULTS[role])
            try:
                values[role] = parse_color(raw)
            except ValueError:
                values[role] = parse_color(DEFAULTS[role])

        custom = curses.can_change_color() and curses.COLORS >= 16 + len(values)
        ids = {}
        next_id = 16
        for role, value in values.items():
            if isinstance(value, int):
                ids[role] = value
            elif custom:
                curses.init_color(next_id, *(round(c * 1000 / 255) for c in value))
                ids[role] = next_id
                next_id += 1
            else:
                ids[role] = _nearest_basic(value)

        self._attrs = {}
        for n, (role, (fg, bg)) in enumerate(ROLE_PAIRS.items(), start=1):
            curses.init_pair(n, ids[fg], ids[bg])
            self._attrs[role] = curses.color_pair(n)
        self._attrs["header"] |= curses.A_BOLD

    def attr(self, role):
        return self._attrs.get(role, 0)
