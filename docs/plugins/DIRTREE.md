# Directory tree (dirtree plugin)

The `dirtree` plugin opens a pane on the left that shows the folders and files of your project. You can open, create, rename, and delete from it.

## Enable and open

```toml
[plugins]
enabled = ["dirtree"]
```

Press **Ctrl+b**, or type `dirtree` in the palette. Press it again, or type `dirtree`, to close the pane. The command `tree` does the same.

The tree opens at the folder of the file you are editing. An unnamed buffer opens at the folder neonano was started from.

## Keys

Keys work when the tree has focus. Press F9 to move focus to it, or open it with Ctrl+b (which gives it focus).

| Key | What it does |
|---|---|
| Up, Down | move the selection |
| Right | expand the selected folder |
| Left | collapse the selected folder |
| Enter | open the selected file in the editor, or expand or collapse a folder |
| `n` | new file in the selected folder (or the selected file's folder) |
| `N` | new folder in the selected folder |
| `r` | rename the selected entry (the current name is shown, edit it, press Enter) |
| `d` | delete the selected entry. You must type `yes` to confirm |
| `.` | show or hide files that start with a dot |
| Esc or F9 | return focus to the editor |

After a change, the new entry is selected. Folders are listed first, then files, each in alphabetical order.

## What you see

```
Dirtree
- src/
    main.c
+ docs/
  README.md
```

`-` marks an expanded folder and `+` a collapsed one. A folder name ends with `/`.

## Notes on operations

- **New file** uses an exclusive create, so it never overwrites an existing file. If the name exists, you get "could not create that name (already exists?)".
- **Names** cannot contain `/`. Use the tree to make folders, one level at a time.
- **Rename** keeps the entry in the same folder. Renaming an expanded folder collapses it.
- **Delete** removes a file, or an empty folder. A folder with contents is refused, with the message "folder is not empty: delete its contents first". Deletion cannot be undone.
- **Open** a file that has unsaved changes is refused. Save first.

## Limits

- No move or copy. Use the shell (in the terminal pane) for those.
- No search and no git status.
- Expanded folders are remembered only for the session.
- The tree lists up to 512 entries per folder and 1024 rows in total.
- No icons: folders are marked with `+` and `-`.
