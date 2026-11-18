"""Word and line counts. Command :wc, action word_count (bound to alt+w by default)."""


def setup(api):
    def count():
        lines = api.buffer.lines
        text = "\n".join(lines)
        api.editor.message(f"lines {len(lines)}  words {len(text.split())}  chars {len(text)}")

    @api.command("wc")
    def cmd_wc(ed, arg):
        count()

    @api.action("word_count")
    def act_wc(ed):
        count()
