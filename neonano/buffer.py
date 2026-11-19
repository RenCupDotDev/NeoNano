class Buffer:
    def __init__(self, lines=None, path=None):
        self.lines = list(lines) if lines else [""]
        self.path = path
        self.row = 0
        self.col = 0
        self.dirty = False
        self.anchor = None  # selection start (row, col), None when no mark is set

    @classmethod
    def open(cls, path):
        try:
            with open(path, encoding="utf-8", newline="") as f:
                text = f.read()
        except FileNotFoundError:
            return cls([""], path)
        text = text.replace("\r\n", "\n")
        lines = text.split("\n")
        if len(lines) > 1 and lines[-1] == "":
            lines.pop()
        return cls(lines, path)

    def save(self, path=None, trim=False, final_newline=True):
        path = path or self.path
        if not path:
            raise ValueError("no file name, use saveto <path>")
        lines = [ln.rstrip() for ln in self.lines] if trim else self.lines
        text = "\n".join(lines) + ("\n" if final_newline else "")
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        self.path = path
        self.dirty = False
        return path

    def snapshot(self):
        return (list(self.lines), self.row, self.col)

    def restore(self, snap):
        lines, row, col = snap
        self.lines = list(lines)
        self.row, self.col = row, col
        self.anchor = None
        self.dirty = True

    @property
    def line(self):
        return self.lines[self.row]

    # ----- movement -----

    def move_to(self, row, col):
        self.row = max(0, min(row, len(self.lines) - 1))
        self.col = max(0, min(col, len(self.lines[self.row])))

    def move_left(self):
        if self.col > 0:
            self.col -= 1
        elif self.row > 0:
            self.row -= 1
            self.col = len(self.line)

    def move_right(self):
        if self.col < len(self.line):
            self.col += 1
        elif self.row < len(self.lines) - 1:
            self.row += 1
            self.col = 0

    def move_up(self):
        self.move_to(self.row - 1, self.col)

    def move_down(self):
        self.move_to(self.row + 1, self.col)

    def line_start(self):
        self.col = 0

    def line_end(self):
        self.col = len(self.line)

    def word_left(self):
        if self.col == 0:
            self.move_left()
            return
        line, i = self.line, self.col
        while i > 0 and line[i - 1].isspace():
            i -= 1
        while i > 0 and not line[i - 1].isspace():
            i -= 1
        self.col = i

    def word_right(self):
        line = self.line
        if self.col >= len(line):
            self.move_right()
            return
        i = self.col
        while i < len(line) and not line[i].isspace():
            i += 1
        while i < len(line) and line[i].isspace():
            i += 1
        self.col = i

    # ----- selection -----

    def selection(self):
        if self.anchor is None:
            return None
        cur = (self.row, self.col)
        if self.anchor == cur:
            return None
        return (self.anchor, cur) if self.anchor < cur else (cur, self.anchor)

    def select_all(self):
        self.anchor = (0, 0)
        self.move_to(len(self.lines) - 1, len(self.lines[-1]))

    def text_in(self, a, b):
        (r1, c1), (r2, c2) = a, b
        if r1 == r2:
            return self.lines[r1][c1:c2]
        parts = [self.lines[r1][c1:]] + self.lines[r1 + 1:r2] + [self.lines[r2][:c2]]
        return "\n".join(parts)

    def delete_range(self, a, b):
        (r1, c1), (r2, c2) = a, b
        merged = self.lines[r1][:c1] + self.lines[r2][c2:]
        self.lines[r1:r2 + 1] = [merged]
        self.row, self.col = r1, c1
        self.anchor = None
        self.dirty = True

    def delete_selection(self):
        sel = self.selection()
        if not sel:
            return False
        self.delete_range(*sel)
        return True

    # ----- editing -----

    def insert(self, text):
        if not text:
            return
        line = self.line
        self.lines[self.row] = line[:self.col] + text + line[self.col:]
        self.col += len(text)
        self.dirty = True

    def insert_text(self, text):
        for i, part in enumerate(text.split("\n")):
            if i:
                self.newline()
            self.insert(part)

    def newline(self, auto_indent=False):
        line = self.line
        indent = ""
        if auto_indent:
            indent = line[: len(line) - len(line.lstrip(" \t"))]
        before, after = line[:self.col], line[self.col:]
        self.lines[self.row:self.row + 1] = [before, indent + after]
        self.row += 1
        self.col = len(indent)
        self.dirty = True

    def backspace(self):
        if self.col > 0:
            line = self.line
            self.lines[self.row] = line[:self.col - 1] + line[self.col:]
            self.col -= 1
        elif self.row > 0:
            prev = self.lines[self.row - 1]
            self.lines[self.row - 1] = prev + self.line
            del self.lines[self.row]
            self.row -= 1
            self.col = len(prev)
        else:
            return
        self.dirty = True

    def delete(self):
        line = self.line
        if self.col < len(line):
            self.lines[self.row] = line[:self.col] + line[self.col + 1:]
        elif self.row < len(self.lines) - 1:
            self.lines[self.row] = line + self.lines[self.row + 1]
            del self.lines[self.row + 1]
        else:
            return
        self.dirty = True

    def delete_line(self):
        if len(self.lines) == 1:
            self.lines = [""]
        else:
            del self.lines[self.row]
        self.move_to(self.row, self.col)
        self.anchor = None
        self.dirty = True

    def duplicate_line(self):
        self.lines.insert(self.row + 1, self.line)
        self.row += 1
        self.anchor = None
        self.dirty = True

    def _rows(self):
        sel = self.selection()
        if sel:
            return range(sel[0][0], sel[1][0] + 1)
        return range(self.row, self.row + 1)

    def indent_rows(self, prefix):
        for r in self._rows():
            if self.lines[r]:
                self.lines[r] = prefix + self.lines[r]
        self.col += len(prefix)
        self.dirty = True

    def unindent_rows(self, width):
        for r in self._rows():
            line = self.lines[r]
            n = 0
            if line.startswith("\t"):
                n = 1
            else:
                while n < width and n < len(line) and line[n] == " ":
                    n += 1
            if n:
                self.lines[r] = line[n:]
                if r == self.row:
                    self.col = max(0, self.col - n)
                self.dirty = True

    # ----- search -----

    def find(self, needle, start_row, start_col):
        """Search forward from the cursor, wrapping around. Returns (row, col) or None."""
        n = len(self.lines)
        for i in range(n + 1):
            r = (start_row + i) % n
            begin = start_col + 1 if i == 0 else 0
            idx = self.lines[r].find(needle, begin)
            if idx != -1:
                return r, idx
        return None
