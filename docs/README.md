# neonano documentation

neonano is a terminal text editor written in C with ncurses. It is nano-like to use, and it is built to be extended: plugins run on one small plugin API.

| Document | Read it if you want to |
|---|---|
| [USER_GUIDE.md](USER_GUIDE.md) | use the editor: keys, commands, panes, configuration |
| [PLUGIN_API.md](PLUGIN_API.md) | write a plugin: every function, key id, and rule of the API |
| [ARCHITECTURE.md](ARCHITECTURE.md) | understand how the code is organised and why |
| [STATUS.md](STATUS.md) | see what is done, what is partial, and what is missing |

Plugins are not part of this repository. To make your own, start from the template repository, `neonano-plugin-template`, which has a working example and a build that needs only the plugin header.

## Quick start

```
git clone <this repo> neonano && cd neonano
make                     # builds ./nnano
./nnano notes.txt        # run the editor
```

To install, run `./install.sh` (see `./install.sh --help`).

Plugins are installed into `~/.nnanoplugins/` (or the system plugin directory) and enabled in `~/.config/neonano/config.toml`:

```toml
[plugins]
enabled = ["name-of-plugin"]
```

Start neonano again. Plugins load only at startup.
