"""Insert saved text snippets. Command :snip <name>, action snippet_prompt (alt+s by default).
Snippets live in resources/snippets.toml, one key per snippet."""
import tomllib


def setup(api):
    with open(api.resource("snippets.toml"), "rb") as f:
        snippets = tomllib.load(f).get("snippets", {})

    def insert(ed, name):
        if name not in snippets:
            ed.message(f"no snippet '{name}' (have: {', '.join(sorted(snippets))})", error=True)
            return
        ed.record(None)
        ed.buf.delete_selection()
        ed.buf.insert_text(snippets[name])
        ed.message(f"inserted {name}")

    @api.command("snip")
    def cmd_snip(ed, arg):
        name = arg.strip() or ed.prompt("snippet: ")
        if name:
            insert(ed, name)

    @api.action("snippet_prompt")
    def act_snip(ed):
        name = ed.prompt("snippet: ")
        if name:
            insert(ed, name)
