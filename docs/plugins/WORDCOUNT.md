# Word count (wordcount plugin)

`wordcount` is the shipped example of a plugin written against the API. It registers the `wc` command.

## Known issue

The core editor already has a `wc` command, so the plugin's registration is refused. The core `wc` does the same job (lines, words, bytes), so you lose nothing, but the plugin is then only a demonstration. Use it as the example it was written to be: look at `plugins/wordcount.c` to see how a command reads the buffer through `line_count` and `line_text`.

## Use

Press F1, type `wc`, press Enter. The status bar shows the number of lines, words, and bytes. Alt+w does the same.
