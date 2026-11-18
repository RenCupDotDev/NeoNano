#!/usr/bin/env bash
# neonano installer for Linux.
#
#   curl -fsSL https://raw.githubusercontent.com/YOUR_GITHUB_USER/neonano/main/install.sh | bash
#   curl -fsSL https://raw.githubusercontent.com/YOUR_GITHUB_USER/neonano/main/install.sh | bash -s -- --user
#
# Or from a local copy:
#   bash install.sh               install (system-wide if run as root, otherwise per-user)
#   bash install.sh --user        per-user install: ~/.local/share/neonano, ~/.local/bin/nnano
#   bash install.sh --system      system install: /usr/local/share/neonano, /usr/local/bin/nnano (sudo)
#   bash install.sh --uninstall   remove neonano, keep your config
#   bash install.sh --purge       remove neonano and your config
#
# Running the installer again updates an existing install. Your config is kept.
set -euo pipefail

APP=neonano
CMD=nnano
REPO="${NEONANO_REPO:-YOUR_GITHUB_USER/neonano}"
BRANCH="${NEONANO_BRANCH:-main}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

MODE=""
ACTION=install
PURGE=0
TMP_SRC=""
LAUNCHER=""

for arg in "$@"; do
  case "$arg" in
    --user) MODE=user ;;
    --system) MODE=system ;;
    --uninstall) ACTION=uninstall ;;
    --purge) ACTION=uninstall; PURGE=1 ;;
    -h|--help) sed -n '2,15p' "$0" 2>/dev/null || echo "see the README"; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 1 ;;
  esac
done

say() { printf '==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

cleanup() {
  [ -n "$TMP_SRC" ] && rm -rf "$TMP_SRC"
  [ -n "$LAUNCHER" ] && rm -f "$LAUNCHER"
  return 0
}
trap cleanup EXIT

if [ -z "$MODE" ]; then
  if [ "$(id -u)" -eq 0 ]; then MODE=system; else MODE=user; fi
fi

SUDO=""
if [ "$MODE" = system ]; then
  if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || die "a system install needs root. Run with sudo, or use --user."
    SUDO=sudo
  fi
  SHARE_DIR=/usr/local/share/$APP
  BIN_DIR=/usr/local/bin
else
  SHARE_DIR=${XDG_DATA_HOME:-$HOME/.local/share}/$APP
  BIN_DIR=$HOME/.local/bin
fi
BIN=$BIN_DIR/$CMD

# Safety: never run rm -rf on an empty or root path.
case "$SHARE_DIR" in
  ""|/|"$HOME") die "refusing to use unsafe path: '$SHARE_DIR'" ;;
esac

# ----- uninstall -----

if [ "$ACTION" = uninstall ]; then
  say "removing $BIN and $SHARE_DIR"
  $SUDO rm -f "$BIN"
  $SUDO rm -rf "$SHARE_DIR"
  if [ "$PURGE" -eq 1 ]; then
    rm -rf "$CONFIG_HOME/$APP"
    say "config removed"
  else
    say "config kept in $CONFIG_HOME/$APP (use --purge to delete it)"
  fi
  say "neonano uninstalled"
  exit 0
fi

# ----- find the source: local copy, or download from GitHub -----

SRC="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || pwd)"
if [ ! -f "$SRC/main.py" ] || [ ! -f "$SRC/VERSION" ]; then
  command -v tar >/dev/null 2>&1 || die "tar is required"
  TMP_SRC="$(mktemp -d)"
  URL="https://github.com/$REPO/archive/refs/heads/$BRANCH.tar.gz"
  say "downloading neonano from $URL"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$URL" | tar -xz -C "$TMP_SRC" --strip-components=1
  elif command -v wget >/dev/null 2>&1; then
    wget -qO- "$URL" | tar -xz -C "$TMP_SRC" --strip-components=1
  else
    die "curl or wget is required to download neonano"
  fi
  SRC="$TMP_SRC"
fi

VERSION="$(cat "$SRC/VERSION" 2>/dev/null || echo unknown)"

# ----- checks -----

command -v python3 >/dev/null 2>&1 || die "python3 not found.
  Fedora:        sudo dnf install python3
  Debian/Ubuntu: sudo apt install python3"

python3 - <<'PY' || die "python3 is too old or has no curses module. neonano needs Python 3.11+ with curses.
  Fedora:        sudo dnf install python3 ncurses
  Debian/Ubuntu: sudo apt install python3 libncurses6"
import sys, curses, tomllib
if sys.version_info < (3, 11):
    raise SystemExit(1)
PY

# ----- install -----

say "installing neonano $VERSION ($MODE) to $SHARE_DIR"
$SUDO rm -rf "$SHARE_DIR"
$SUDO mkdir -p "$SHARE_DIR" "$BIN_DIR"
$SUDO cp -a "$SRC"/. "$SHARE_DIR"/
$SUDO find "$SHARE_DIR" -name __pycache__ -type d -prune -exec rm -rf {} +
$SUDO rm -f "$SHARE_DIR/install.sh"

PY="$(command -v python3)"
LAUNCHER="$(mktemp)"
cat > "$LAUNCHER" <<LAUNCH
#!/bin/sh
# $CMD: starts neonano. Installed by install.sh.
case "\$1" in
  --version) echo "neonano $VERSION"; exit 0 ;;
esac
exec "$PY" "$SHARE_DIR/main.py" "\$@"
LAUNCH
$SUDO install -m 755 "$LAUNCHER" "$BIN"

say "running a smoke test"
"$BIN" --version

case ":$PATH:" in
  *":$BIN_DIR:"*) ;;
  *) printf '\nnote: %s is not in your PATH. Add this line to ~/.bashrc or ~/.zshrc:\n  export PATH="%s:$PATH"\n' "$BIN_DIR" "$BIN_DIR" ;;
esac

cat <<DONE

neonano $VERSION is installed.
  start:   $CMD [file]
  config:  $CONFIG_HOME/$APP/config.toml   (created on first run)
  update:  rerun the install command
  remove:  curl -fsSL https://raw.githubusercontent.com/$REPO/$BRANCH/install.sh | bash -s -- --uninstall
DONE
