#!/usr/bin/env bash
#
# Copyright (C) 2026 RenCupDotDev
#
# This file is part of neonano.
#
# neonano is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version. It is distributed WITHOUT ANY WARRANTY; see the GNU General Public
# License for details: <https://www.gnu.org/licenses/>.
#
# install.sh: install, update and remove NeoNano and the NeoNanoQOL plugins.
#
# It needs no source code. Everything comes from the GitHub releases:
#   - the release JSON from the GitHub API (file names, download URLs and the
#     sha256 GitHub publishes for every asset),
#   - the assets themselves, each checked against its sha256 before it is used.
#
# With --git it clones the NeoNano and NeoNanoQOL repositories and compiles
# them instead. A git build has no release digest to check against, so it
# builds exactly the default branch it cloned over HTTPS from GitHub.
#
#   curl -fsSL https://raw.githubusercontent.com/RenCupDotDev/NeoNano/main/install.sh | bash

set -euo pipefail

NEONANO_REPO="RenCupDotDev/NeoNano"
QOL_REPO="RenCupDotDev/NeoNanoQOL"

# GitHub API base. NEONANO_GITHUB_API replaces it (a mirror, or a test server).
GITHUB_API="${NEONANO_GITHUB_API:-https://api.github.com}"

# Git base for --git. NEONANO_GIT_BASE replaces https://github.com (a mirror, or a test).
GIT_BASE="${NEONANO_GIT_BASE:-https://github.com}"

# ---- options ----------------------------------------------------------------
MODE=user            # user | system
ACTION=install       # install | uninstall
PREFIX_OPT=""        # --prefix, for --system
PURGE=0
DRY=0
YES=0
UPDATE=0             # --update: apply available updates without asking
QOL_CHOICE=ask       # ask | yes | no
QOL_PLUGINS=""       # comma-separated names, or "all"
NN_TAG=""            # empty: the latest NeoNano release
QOL_TAG=""           # empty: the latest NeoNanoQOL release
GIT_BUILD=0          # --git: clone and compile instead of using the releases

usage() {
    cat <<'EOF'
Usage: install.sh [options]

  (no option)            install NeoNano; update it if a newer release exists
  --update               same, and apply updates without asking
  --system               install system-wide (needs root, or --prefix DIR)
  --prefix DIR           root of a --system install (default /usr/local)
  --qol, --no-qol        install / skip the NeoNanoQOL plugins (default: ask)
  --qol-plugins LIST     comma-separated plugin names, or "all"
  --neonano-tag TAG      install this NeoNano release instead of the latest
  --qol-tag TAG          install this NeoNanoQOL release instead of the latest
  --git                  clone and compile NeoNano and the plugins (needs git, make, cc)
  --uninstall            remove what the last install put in place
  --purge                uninstall and also delete the config and state
  -n, --dry-run          print what would happen; change nothing
  -y, --yes              don't ask questions (answers the default)
  -h, --help             this text
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --user)           MODE=user ;;
        --system)         MODE=system ;;
        --prefix)         PREFIX_OPT="${2:?--prefix needs a directory}"; shift ;;
        --update)         UPDATE=1 ;;
        --uninstall)      ACTION=uninstall ;;
        --purge)          ACTION=uninstall; PURGE=1 ;;
        -n|--dry-run)     DRY=1 ;;
        -y|--yes)         YES=1 ;;
        --qol)            QOL_CHOICE=yes ;;
        --no-qol)         QOL_CHOICE=no ;;
        --qol-plugins)    QOL_PLUGINS="${2:?--qol-plugins needs a list}"; shift ;;
        --neonano-tag)    NN_TAG="${2:?--neonano-tag needs a tag}"; shift ;;
        --qol-tag)        QOL_TAG="${2:?--qol-tag needs a tag}"; shift ;;
        --git)            GIT_BUILD=1 ;;
        -h|--help)        usage; exit 0 ;;
        *)                echo "error: unknown option: $1 (try --help)" >&2; exit 1 ;;
    esac
    shift
done

if [ "$QOL_CHOICE" = ask ] && [ -n "$QOL_PLUGINS" ]; then QOL_CHOICE=yes; fi

# ---- output and helpers -----------------------------------------------------
log()  { printf '%s\n' "$*"; }
warn() { printf 'warning: %s\n' "$*" >&2; }
die()  { printf 'error: %s\n' "$*" >&2; exit 1; }
step() { printf '\n==> %s\n' "$*"; }
need() { command -v "$1" >/dev/null 2>&1 || die "missing tool: $1"; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
: > "$TMP/record"       # files and config entries this run puts in place
: > "$TMP/qol_record"   # QOL lines for the install record
: > "$TMP/nn_record"    # NeoNano lines for the install record

# Runs a command, or only prints it in dry-run mode.
run() {
    if [ "$DRY" = 1 ]; then
        printf '  [dry-run] %s\n' "$*"
    else
        "$@"
    fi
}

# Questions are read from the terminal, so this works when piped into bash.
has_tty() { { : </dev/tty; } 2>/dev/null; }

# ask "question" y|n : the default is used with --yes or without a terminal.
ask() {
    local def="$2" ans=""
    if [ "$YES" = 1 ] || ! has_tty; then [ "$def" = y ]; return; fi
    if [ "$def" = y ]; then
        read -r -p "$1 [Y/n] " ans </dev/tty || true
    else
        read -r -p "$1 [y/N] " ans </dev/tty || true
    fi
    ans="${ans:-$def}"
    case "$ans" in y|Y|yes) return 0 ;; *) return 1 ;; esac
}

# confirm "question": for deleting things. Needs --yes or a terminal answer of yes.
confirm() {
    [ "$YES" = 1 ] && return 0
    has_tty || return 1
    local ans=""
    read -r -p "$1 [y/N] " ans </dev/tty || true
    case "$ans" in y|Y|yes) return 0 ;; *) return 1 ;; esac
}

check_platform() {
    need install; need awk; need sed; need grep; need tr; need cut; need mktemp
    if [ "$GIT_BUILD" = 1 ]; then
        need git; need make; need cc
    else
        need curl; need jq; need sha256sum
        case "$(uname -m)" in
            x86_64|amd64) ;;
            *) die "there is no NeoNano release build for $(uname -m): x86_64 only (use --git to compile it)" ;;
        esac
    fi
    [ "$(uname -s)" = Linux ] || die "Linux only"
    if [ "$MODE" = system ]; then need getent; fi
    if command -v ldconfig >/dev/null 2>&1; then
        ldconfig -p > "$TMP/ld.txt" 2>/dev/null || true
        grep -q 'libncursesw\.so' "$TMP/ld.txt" \
            || warn "libncursesw was not found: install the ncurses wide-character runtime"
    fi
}

# ---- where things go -------------------------------------------------------
setup_paths() {
    local owner_home="$HOME"
    if [ "$MODE" = system ]; then
        SYS_PREFIX="${PREFIX_OPT:-/usr/local}"
        BIN_DIR="$SYS_PREFIX/bin"
        PLUGIN_DIR="$SYS_PREFIX/lib/neonano/plugins"
        STATE_DIR="$SYS_PREFIX/share/neonano"
        if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
            owner_home="$(getent passwd "$SUDO_USER" | cut -d: -f6)"
        fi
    else
        BIN_DIR="$HOME/.local/bin"
        PLUGIN_DIR="$HOME/.nnanoplugins"
        STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/neonano"
    fi
    CONFIG_DIR="${XDG_CONFIG_HOME:-$owner_home/.config}/neonano"
    CFG="$CONFIG_DIR/config.toml"
    MANIFEST="$STATE_DIR/install.manifest"
    BACKUP_DIR="$STATE_DIR/backup"
    if [ -f "$MANIFEST" ]; then cp "$MANIFEST" "$TMP/old_manifest"; else : > "$TMP/old_manifest"; fi
}

check_system_write() {
    [ "$MODE" = system ] && [ "$DRY" = 0 ] && [ "$(id -u)" != 0 ] || return 0
    local probe="$SYS_PREFIX"
    while [ ! -e "$probe" ]; do probe="$(dirname "$probe")"; done
    [ -w "$probe" ] || die "a system install needs root (run with sudo) or a writable --prefix"
}

# ---- the install record -----------------------------------------------------
# $MANIFEST holds: file=, backup=DEST|BACKUP, enabled=NAME, nn_version=,
# qol_version=, qol_plugin=, mode=, config=.
manifest_all() { grep -E "^$1=" "$MANIFEST" 2>/dev/null | cut -d= -f2- || true; }
old_all()      { grep -E "$1" "$TMP/old_manifest" 2>/dev/null || true; }

note() { printf '%s\n' "$1" >> "$TMP/record"; }

# True if this run, or the run before, put this file in place.
ours() {
    grep -qxF "file=$1" "$TMP/record" 2>/dev/null || grep -qxF "file=$1" "$TMP/old_manifest" 2>/dev/null
}

# place_file SRC DEST BACKUPNAME MODE: a file that was there before and is not
# ours is kept in the backup folder, and is put back on uninstall.
place_file() {
    local src="$1" dest="$2" name="$3" mode="$4"
    if [ -e "$dest" ] && ! ours "$dest"; then
        local bak="$BACKUP_DIR/$name"
        warn "$dest exists and was not installed by neonano: keeping a copy in $bak"
        run mkdir -p "$BACKUP_DIR"
        run cp -p "$dest" "$bak"
        note "backup=$dest|$bak"
    fi
    run install -m "$mode" "$src" "$dest"
    note "file=$dest"
}

write_manifest() {
    if [ "$DRY" = 1 ]; then
        log "  [dry-run] would update the install record in $MANIFEST"
        return 0
    fi
    mkdir -p "$STATE_DIR"
    {
        old_all '^(file|backup|enabled)='
        cat "$TMP/record"
        if [ -s "$TMP/qol_record" ]; then cat "$TMP/qol_record"; else old_all '^qol_'; fi
        if [ -s "$TMP/nn_record" ]; then cat "$TMP/nn_record"; else old_all '^nn_version='; fi
        printf 'mode=%s\nconfig=%s\n' "$MODE" "$CFG"
    } | grep -v '^$' | awk '!seen[$0]++' > "$TMP/manifest.new"
    cat "$TMP/manifest.new" > "$MANIFEST"
}

# ---- config.toml ------------------------------------------------------------
# The names in [plugins] enabled, one per line.
config_names() {
    [ -f "$CFG" ] || return 0
    awk '/^\[/{sec=$0} sec=="[plugins]" && /^[[:space:]]*enabled[[:space:]]*=/{print; exit}' "$CFG" \
        | sed -E 's/^[^[]*\[//; s/\].*$//' | tr ',' '\n' | tr -d ' "' | grep -v '^$' || true
}

# set_config_names NAME...: make [plugins] enabled exactly this list.
set_config_names() {
    local line="" n
    for n in "$@"; do line+="${line:+, }\"$n\""; done
    line="enabled = [$line]"

    if [ ! -f "$CFG" ]; then
        if [ "$DRY" = 1 ]; then log "  [dry-run] would create $CFG with: $line"; return 0; fi
        mkdir -p "$CONFIG_DIR"
        printf '[plugins]\n%s\n' "$line" > "$CFG"
        return 0
    fi

    local out="$TMP/config.new"
    awk -v line="$line" '
        BEGIN { insec = 0; done = 0; have = 0 }
        /^\[/ {
            if (insec && !done) { print line; done = 1 }
            insec = ($0 == "[plugins]")
            if (insec) have = 1
            print
            next
        }
        insec && !done && /^[[:space:]]*enabled[[:space:]]*=/ { print line; done = 1; next }
        { print }
        END {
            if (insec && !done) { print line; done = 1 }
            if (!have) { print ""; print "[plugins]"; print line }
        }
    ' "$CFG" > "$out"

    if [ "$DRY" = 1 ]; then log "  [dry-run] would set in $CFG: $line"; return 0; fi
    cp -p "$CFG" "$CFG.bak"
    cat "$out" > "$CFG"
}

# Enable plugins. Only names that were not enabled before are recorded, so
# uninstall never turns off something the user had on already.
enable_plugins() {
    [ $# -gt 0 ] || return 0
    local -a cur=() add=()
    local n c found
    mapfile -t cur < <(config_names)
    for n in "$@"; do
        found=0
        for c in "${cur[@]}"; do [ "$c" = "$n" ] && found=1; done
        if [ "$found" = 0 ]; then add+=("$n"); note "enabled=$n"; fi
    done
    set_config_names "${cur[@]}" "${add[@]}"
}

disable_plugins() {
    [ $# -gt 0 ] || return 0
    [ -f "$CFG" ] || return 0
    local -a cur=() keep=()
    local c n drop
    mapfile -t cur < <(config_names)
    for c in "${cur[@]}"; do
        drop=0
        for n in "$@"; do [ "$c" = "$n" ] && drop=1; done
        [ "$drop" = 0 ] && keep+=("$c")
    done
    set_config_names "${keep[@]}"
}

# ---- GitHub releases --------------------------------------------------------
# release_json REPO TAG OUTFILE: the release (latest, or TAG) as JSON.
release_json() {
    local repo="$1" tag="$2" out="$3" url
    if [ -n "$tag" ]; then
        url="$GITHUB_API/repos/$repo/releases/tags/$tag"
    else
        url="$GITHUB_API/repos/$repo/releases/latest"
    fi
    curl -fsSL -H 'Accept: application/vnd.github+json' -o "$out" "$url" \
        || die "could not read the release of $repo from GitHub ($url)"
    jq -e '.assets' "$out" >/dev/null 2>&1 || die "GitHub returned no release for $repo"
}

# download_asset JSON NAME DEST: fetch an asset and check it against the
# sha256 GitHub publishes for it. A file that does not match is deleted.
download_asset() {
    local json="$1" name="$2" dest="$3" url want got
    url="$(jq -r --arg n "$name" '.assets[] | select(.name == $n) | .browser_download_url' "$json")"
    [ -n "$url" ] || die "the release has no file named $name"
    want="$(jq -r --arg n "$name" '.assets[] | select(.name == $n) | (.digest // "")' "$json" | sed 's/^sha256://')"
    [ -n "$want" ] || die "GitHub gives no sha256 for $name: refusing to install it"
    curl -fsSL --retry 2 -m 600 -o "$dest" "$url" || die "download failed: $name"
    got="$(sha256sum "$dest" | cut -d' ' -f1)"
    if [ "$got" != "$want" ]; then
        rm -f "$dest"
        die "sha256 mismatch for $name (GitHub: $want, download: $got): nothing was installed"
    fi
    log "  $name: sha256 OK"
}

# ---- NeoNano ----------------------------------------------------------------
# git_clone REPO DIR: a shallow clone of the default branch of REPO into DIR.
git_clone() {
    local repo="$1" dir="$2"
    log "  cloning $repo"
    git clone --quiet --depth 1 "$GIT_BASE/$repo.git" "$dir" \
        || die "could not clone $GIT_BASE/$repo.git"
}

installed_nn_version() {
    local b="$BIN_DIR/nnano"
    [ -x "$b" ] || return 0
    "$b" --version 2>/dev/null | awk '{print $NF}' || true
}

do_neonano() {
    step "NeoNano"
    local new cur="" bin="$BIN_DIR/nnano" src_bin=""
    if [ "$GIT_BUILD" = 1 ]; then
        log "building NeoNano from git"
        git_clone "$NEONANO_REPO" "$TMP/nn-src"
        new="$(tr -d '[:space:]' < "$TMP/nn-src/VERSION")"
    else
        release_json "$NEONANO_REPO" "$NN_TAG" "$TMP/nn.json"
        download_asset "$TMP/nn.json" VERSION "$TMP/nn-VERSION"
        new="$(tr -d '[:space:]' < "$TMP/nn-VERSION")"
    fi

    if [ -e "$bin" ]; then cur="$(installed_nn_version)"; cur="${cur:-unknown}"; fi
    if [ -z "$cur" ]; then
        log "NeoNano $new is not installed: installing it"
    elif [ "$cur" = "$new" ]; then
        log "NeoNano $cur is up to date"
        return 0
    else
        log "NeoNano $cur is installed; $new is the latest release"
        if [ "$UPDATE" = 0 ] && ! ask "Update NeoNano to $new?" y; then return 0; fi
    fi

    if [ "$GIT_BUILD" = 1 ]; then
        make -s -C "$TMP/nn-src" nnano || die "compiling NeoNano failed"
        src_bin="$TMP/nn-src/nnano"
    else
        download_asset "$TMP/nn.json" nnano "$TMP/nnano"
        src_bin="$TMP/nnano"
    fi
    run install -d "$BIN_DIR"
    place_file "$src_bin" "$bin" nnano 755
    printf 'nn_version=%s\n' "$new" >> "$TMP/nn_record"
    NN_NOW="$new"
}

# ---- NeoNanoQOL -------------------------------------------------------------
CHOSEN=()

# choose_plugins AVAILABLE...: sets CHOSEN from --qol-plugins, or asks, or takes all.
choose_plugins() {
    local -a avail=("$@") pick=()
    local n p i reply found
    CHOSEN=()
    if [ -n "$QOL_PLUGINS" ] && [ "$QOL_PLUGINS" != all ]; then
        IFS=',' read -ra pick <<< "$QOL_PLUGINS"
        for p in "${pick[@]}"; do
            p="${p// /}"
            found=0
            for n in "${avail[@]}"; do [ "$n" = "$p" ] && found=1; done
            [ "$found" = 1 ] || die "$p is not a plugin in the NeoNanoQOL release"
            CHOSEN+=("$p")
        done
    elif [ "$QOL_PLUGINS" != all ] && [ "$YES" = 0 ] && has_tty; then
        log "NeoNanoQOL plugins:"
        i=1
        for n in "${avail[@]}"; do printf '  %d) %s\n' "$i" "$n"; i=$((i + 1)); done
        read -r -p "Which ones? Numbers separated by spaces, or Enter for all: " reply </dev/tty || reply=""
        if [ -z "${reply// /}" ]; then
            CHOSEN=("${avail[@]}")
        else
            for p in $reply; do
                [[ "$p" =~ ^[0-9]+$ ]] && [ "$p" -ge 1 ] && [ "$p" -le "${#avail[@]}" ] \
                    || die "not a number from the list: $p"
                CHOSEN+=("${avail[$((p - 1))]}")
            done
        fi
    else
        CHOSEN=("${avail[@]}")
    fi
}

do_qol() {
    step "NeoNanoQOL"
    local new cur
    if [ "$GIT_BUILD" = 1 ]; then
        log "building NeoNanoQOL from git"
        git_clone "$QOL_REPO" "$TMP/qol-src"
        make -s -C "$TMP/qol-src" || die "compiling NeoNanoQOL failed"
        new="$(tr -d '[:space:]' < "$TMP/qol-src/VERSION")"
    else
        release_json "$QOL_REPO" "$QOL_TAG" "$TMP/qol.json"
        download_asset "$TMP/qol.json" VERSION "$TMP/qol-VERSION"
        new="$(tr -d '[:space:]' < "$TMP/qol-VERSION")"
    fi
    cur="$(manifest_all qol_version | head -n1)"

    # The plugins are the .so files of the release, or of the build. Names are checked before use.
    local -a avail=() prev=() todo=()
    if [ "$GIT_BUILD" = 1 ]; then
        local f
        for f in "$TMP/qol-src/build"/*.so; do
            [ -e "$f" ] && avail+=("$(basename "$f" .so)")
        done
    else
        mapfile -t avail < <(jq -r '.assets[].name | select(endswith(".so")) | rtrimstr(".so")' "$TMP/qol.json")
    fi
    [ "${#avail[@]}" -gt 0 ] || die "the NeoNanoQOL release has no .so plugins"
    local a
    for a in "${avail[@]}"; do
        [[ "$a" =~ ^[A-Za-z0-9_-]+$ ]] || die "refusing plugin name: $a"
    done
    mapfile -t prev < <(manifest_all qol_plugin)

    if [ -z "$cur" ]; then
        log "installing NeoNanoQOL $new"
        choose_plugins "${avail[@]}"
        todo=("${CHOSEN[@]}")
    elif [ -z "$QOL_PLUGINS" ] && [ "$cur" = "$new" ]; then
        log "NeoNanoQOL $cur is up to date"
        return 0
    else
        if [ "$cur" != "$new" ]; then
            log "NeoNanoQOL $cur is installed; $new is the latest release"
            if [ "$UPDATE" = 0 ] && ! ask "Update NeoNanoQOL to $new?" y; then return 0; fi
        fi
        if [ -n "$QOL_PLUGINS" ]; then
            choose_plugins "${avail[@]}"
            todo=("${CHOSEN[@]}")
        else
            todo=("${prev[@]}")
        fi
    fi
    if [ "${#todo[@]}" -eq 0 ]; then
        log "no plugins to install"
        return 0
    fi

    local n
    for n in "${todo[@]}"; do
        if [ "$GIT_BUILD" = 1 ]; then
            cp "$TMP/qol-src/build/$n.so" "$TMP/qol-$n.so"
        else
            download_asset "$TMP/qol.json" "$n.so" "$TMP/qol-$n.so"
        fi
    done
    run install -d "$PLUGIN_DIR"
    for n in "${todo[@]}"; do
        place_file "$TMP/qol-$n.so" "$PLUGIN_DIR/$n.so" "$n.so" 644
        printf 'qol_plugin=%s\n' "$n" >> "$TMP/qol_record"
    done
    printf 'qol_version=%s\n' "$new" >> "$TMP/qol_record"
    enable_plugins "${todo[@]}"
    log "NeoNanoQOL $new: ${todo[*]}"
}

# ---- actions ----------------------------------------------------------------
install_flow() {
    check_platform
    check_system_write

    if [ "$QOL_CHOICE" = ask ]; then
        if [ -n "$(manifest_all qol_version)" ]; then
            QOL_CHOICE=yes
        elif ask "Also install the NeoNanoQOL plugins?" n; then
            QOL_CHOICE=yes
        else
            QOL_CHOICE=no
        fi
    fi

    do_neonano
    if [ "$QOL_CHOICE" = yes ]; then do_qol; fi
    write_manifest

    step "Done"
    if [ -n "${NN_NOW:-}" ]; then log "NeoNano is now $NN_NOW at $BIN_DIR/nnano"; fi
    case ":$PATH:" in *":$BIN_DIR:"*) ;; *) log "note: $BIN_DIR is not on your PATH" ;; esac
    if [ "$DRY" = 1 ]; then log "(dry run: nothing was changed)"; fi
    return 0
}

uninstall_flow() {
    if [ ! -f "$MANIFEST" ]; then
        [ "$PURGE" = 1 ] || die "nothing to uninstall: no install record at $MANIFEST"
        warn "no install record at $MANIFEST: only the settings are removed"
    fi

    local recorded_cfg
    if [ -f "$MANIFEST" ]; then
        recorded_cfg="$(manifest_all config | head -n1)"
        if [ -n "$recorded_cfg" ]; then CFG="$recorded_cfg"; fi
    fi

    step "Removing neonano"
    local f entry dest bak
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        if [ -e "$f" ] || [ -L "$f" ]; then run rm -f "$f"; fi
    done < <(manifest_all file)

    while IFS= read -r entry; do
        [ -n "$entry" ] || continue
        dest="${entry%%|*}"
        bak="${entry#*|}"
        if [ -f "$bak" ]; then
            log "restoring $dest"
            run cp -p "$bak" "$dest"
            run rm -f "$bak"
        fi
    done < <(manifest_all backup)

    local -a names=()
    mapfile -t names < <(manifest_all enabled)
    if [ "${#names[@]}" -gt 0 ]; then disable_plugins "${names[@]}"; fi

    run rmdir "$BACKUP_DIR" 2>/dev/null || true
    run rmdir "$PLUGIN_DIR" 2>/dev/null || true
    if [ -f "$MANIFEST" ]; then run rm -f "$MANIFEST"; fi

    if [ "$PURGE" = 1 ]; then
        step "Purging settings"
        if confirm "Delete $CONFIG_DIR (config.toml, themes and keys) and $STATE_DIR?"; then
            run rm -rf "$CONFIG_DIR"
            run rm -rf "$STATE_DIR"
        else
            log "kept $CONFIG_DIR and $STATE_DIR (use --yes to delete them without asking)"
        fi
    fi
    step "Done"
    if [ "$DRY" = 1 ]; then log "(dry run: nothing was changed)"; fi
    return 0
}

setup_paths
case "$ACTION" in
    install)   install_flow ;;
    uninstall) uninstall_flow ;;
esac
