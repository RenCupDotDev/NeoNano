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
# install.sh: build, install, update and remove neonano, and optionally install
# the NeoNanoQOL plugins and enable them in config.toml.
#
# Run it from the neonano source tree. The only value written into this script
# is the NeoNanoQOL repository. Everything else is read when it runs: the
# neonano version from the VERSION file, the QOL version, the list of QOL
# plugins and their checksums from the GitHub release.

set -euo pipefail

QOL_REPO_DEFAULT="RenCupDotDev/NeoNanoQOL"

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---- options ----------------------------------------------------------------
MODE=user               # user | system
ACTION=install          # install | update | uninstall
PREFIX_OPT=""           # --prefix for --system
PURGE=0
DRY=0
YES=0
QOL_CHOICE=ask          # ask | yes | no
QOL_REPO="$QOL_REPO_DEFAULT"
QOL_TAG=""              # empty or "latest": newest release
QOL_PLUGINS=""          # comma-separated names, or "all"
QOL_DIR=""              # install QOL files from a local folder instead
ACCEPT_UNVERIFIED=0

usage() {
    cat <<'EOF'
Usage: ./install.sh [options]

  (no option)            install neonano for the current user
  --system               install system-wide (needs root, or --prefix DIR)
  --prefix DIR           root of a --system install (default /usr/local)
  --update               git pull (if a git checkout), rebuild, reinstall;
                         QOL plugins are refreshed from their last settings
  --uninstall            remove what the last install put in place
  --purge                uninstall and also delete the config and state
  -n, --dry-run          print what would happen; change nothing
  -y, --yes              don't ask questions (purge needs this when not on a terminal)
  --qol, --no-qol        install / skip the NeoNanoQOL plugins (default: ask)
  --qol-version TAG      QOL release tag (default: latest)
  --qol-plugins LIST     comma-separated plugin names, or "all" (default: ask)
  --qol-repo OWNER/NAME  use another repository for QOL
  --qol-dir DIR          take the QOL files from a local folder (no download)
  --accept-unverified    install QOL files when the release has no SHA256SUMS
  -h, --help             this text
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --user)               MODE=user ;;
        --system)             MODE=system ;;
        --prefix)             PREFIX_OPT="${2:?--prefix needs a directory}"; shift ;;
        --update)             ACTION=update ;;
        --uninstall)          ACTION=uninstall ;;
        --purge)              ACTION=uninstall; PURGE=1 ;;
        -n|--dry-run)         DRY=1 ;;
        -y|--yes)             YES=1 ;;
        --qol)                QOL_CHOICE=yes ;;
        --no-qol)             QOL_CHOICE=no ;;
        --qol-version)        QOL_TAG="${2:?--qol-version needs a tag}"; shift ;;
        --qol-plugins)        QOL_PLUGINS="${2:?--qol-plugins needs a list}"; shift ;;
        --qol-repo)           QOL_REPO="${2:?--qol-repo needs OWNER/NAME}"; shift ;;
        --qol-dir)            QOL_DIR="${2:?--qol-dir needs a folder}"; shift ;;
        --accept-unverified)  ACCEPT_UNVERIFIED=1 ;;
        -h|--help)            usage; exit 0 ;;
        *)                    echo "error: unknown option: $1 (try --help)" >&2; exit 1 ;;
    esac
    shift
done

if [ "$QOL_CHOICE" = ask ] && { [ -n "$QOL_PLUGINS" ] || [ -n "$QOL_DIR" ] || [ -n "$QOL_TAG" ]; }; then
    QOL_CHOICE=yes
fi

# ---- output -----------------------------------------------------------------
log()  { printf '%s\n' "$*"; }
warn() { printf 'warning: %s\n' "$*" >&2; }
die()  { printf 'error: %s\n' "$*" >&2; exit 1; }
step() { printf '\n==> %s\n' "$*"; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
: > "$TMP/record"          # files and config entries this run puts in place

# Runs a command, or only prints it in dry-run mode.
run() {
    if [ "$DRY" = 1 ]; then
        printf '  [dry-run] %s\n' "$*"
    else
        "$@"
    fi
}

interactive() { [ -t 0 ] && [ "$YES" = 0 ]; }

# confirm "question": destructive steps. Default no; yes with --yes.
confirm() {
    [ "$YES" = 1 ] && return 0
    interactive || return 1
    local ans
    read -r -p "$1 [y/N] " ans || ans=""
    case "$ans" in y|Y|yes) return 0 ;; *) return 1 ;; esac
}

# ask_yes "question" y|n: optional choices. Default when not interactive.
ask_yes() {
    local def="$2" ans
    if ! interactive; then [ "$def" = y ]; return; fi
    if [ "$def" = y ]; then read -r -p "$1 [Y/n] " ans || ans=""; else read -r -p "$1 [y/N] " ans || ans=""; fi
    ans="${ans:-$def}"
    case "$ans" in y|Y|yes) return 0 ;; *) return 1 ;; esac
}

need() { command -v "$1" >/dev/null 2>&1 || die "missing tool: $1"; }

# ---- paths ------------------------------------------------------------------
setup_paths() {
    local owner_home="$HOME"
    if [ "$MODE" = system ]; then
        SYS_PREFIX="${PREFIX_OPT:-/usr/local}"
        BIN_DIR="$SYS_PREFIX/bin"
        PLUGIN_DIR="$SYS_PREFIX/lib/neonano/plugins"
        STATE_DIR="$SYS_PREFIX/share/neonano"
        BUILD_PLUGINDIR="$PLUGIN_DIR"
        if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
            owner_home="$(getent passwd "$SUDO_USER" | cut -d: -f6)"
        fi
    else
        SYS_PREFIX=""
        BIN_DIR="$HOME/.local/bin"
        PLUGIN_DIR="$HOME/.nnanoplugins"
        STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/neonano"
        BUILD_PLUGINDIR=""
    fi
    CONFIG_DIR="${XDG_CONFIG_HOME:-$owner_home/.config}/neonano"
    CFG="$CONFIG_DIR/config.toml"
    MANIFEST="$STATE_DIR/install.manifest"
    BACKUP_DIR="$STATE_DIR/backup"
    if [ -f "$MANIFEST" ]; then cp "$MANIFEST" "$TMP/old_manifest"; else : > "$TMP/old_manifest"; fi
}

# ---- install record ---------------------------------------------------------
# $MANIFEST holds: file=PATH, backup=DEST|BACKUP, enabled=NAME, qol_*=...,
# mode=..., config=PATH. Lines are read with these helpers.
manifest_all() { grep -E "^$1=" "$MANIFEST" 2>/dev/null | cut -d= -f2- || true; }
old_all()      { grep -E "^$1=" "$TMP/old_manifest" 2>/dev/null | cut -d= -f2- || true; }

note() { printf '%s\n' "$1" >> "$TMP/record"; }

# True if this install (or the one before) put this file in place itself.
ours() {
    grep -qxF "file=$1" "$TMP/record" 2>/dev/null || grep -qxF "file=$1" "$TMP/old_manifest" 2>/dev/null
}

# place_file SRC DEST BACKUPNAME: copy SRC over DEST. A file that was there
# before and is not ours is kept in the backup folder and restored on uninstall.
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
    if [ "$DRY" = 1 ]; then log "  [dry-run] would write the install record to $MANIFEST"; return; fi
    mkdir -p "$STATE_DIR"
    {
        grep -E '^(file|backup|enabled)=' "$TMP/old_manifest" 2>/dev/null || true
        cat "$TMP/record"
        if [ -s "$TMP/qol_record" ]; then
            cat "$TMP/qol_record"
        else
            grep -E '^qol_' "$TMP/old_manifest" 2>/dev/null || true
        fi
        printf 'mode=%s\nconfig=%s\n' "$MODE" "$CFG"
    } | grep -v '^$' | awk '!seen[$0]++' > "$TMP/manifest.new"
    cat "$TMP/manifest.new" > "$MANIFEST"
}

# ---- config.toml ------------------------------------------------------------
# Names currently in [plugins] enabled, one per line.
config_names() {
    [ -f "$CFG" ] || return 0
    awk '/^\[/{sec=$0} sec=="[plugins]" && /^[[:space:]]*enabled[[:space:]]*=/{print; exit}' "$CFG" \
        | sed -E 's/^[^[]*\[//; s/\].*$//' | tr ',' '\n' | tr -d ' "' | grep -v '^$' || true
}

# set_config_names NAME...: rewrite the [plugins] enabled line to exactly these names.
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

# Enable the given plugins. Only names that were not enabled before are
# recorded, so uninstall never disables something the user had on already.
enable_plugins() {
    [ $# -gt 0 ] || return 0
    local cur=() add=() n
    mapfile -t cur < <(config_names)
    for n in "$@"; do
        local found=0 c
        for c in "${cur[@]}"; do [ "$c" = "$n" ] && found=1; done
        [ "$found" = 0 ] && add+=("$n") && note "enabled=$n"
    done
    set_config_names "${cur[@]}" "${add[@]}"
}

disable_plugins() {
    [ $# -gt 0 ] || return 0
    local cur=() keep=() c n
    mapfile -t cur < <(config_names)
    for c in "${cur[@]}"; do
        local drop=0
        for n in "$@"; do [ "$c" = "$n" ] && drop=1; done
        [ "$drop" = 0 ] && keep+=("$c")
    done
    [ -f "$CFG" ] || return 0
    set_config_names "${keep[@]}"
}

# ---- NeoNanoQOL -------------------------------------------------------------
QDL="$TMP/qol"
QOL_SOURCE=""
QOL_BASE=""
QOL_TAG_RESOLVED=""
QOL_AVAIL=()
SELECTED=()

# fetch_qol NAME DEST REQUIRED(1|0)
fetch_qol() {
    local name="$1" dest="$2" required="$3"
    if [ -n "$QOL_DIR" ]; then
        if [ -f "$QOL_DIR/$name" ]; then cp "$QOL_DIR/$name" "$dest"; return 0; fi
    else
        if curl -fsSL -m 600 -o "$dest" "$QOL_BASE/$name"; then return 0; fi
    fi
    rm -f "$dest"
    if [ "$required" = 1 ]; then die "could not get $name from $QOL_SOURCE"; fi
    return 1
}

# Work out the release tag and the plugin names, and fetch VERSION and SHA256SUMS.
qol_discover() {
    mkdir -p "$QDL"
    if [ -n "$QOL_DIR" ]; then
        [ -d "$QOL_DIR" ] || die "--qol-dir is not a folder: $QOL_DIR"
        QOL_SOURCE="folder $QOL_DIR"
        fetch_qol VERSION "$QDL/VERSION" 1
        fetch_qol SHA256SUMS "$QDL/SHA256SUMS" 0 || true
        shopt -s nullglob
        local f
        for f in "$QOL_DIR"/*.so; do QOL_AVAIL+=("$(basename "$f" .so)"); done
        shopt -u nullglob
        QOL_TAG_RESOLVED="$(tr -d '[:space:]' < "$QDL/VERSION")"
        return 0
    fi

    [[ "$QOL_REPO" =~ ^[A-Za-z0-9._-]+/[A-Za-z0-9._-]+$ ]] || die "--qol-repo must look like OWNER/NAME"
    QOL_SOURCE="$QOL_REPO"
    local api="https://api.github.com/repos/$QOL_REPO/releases" url
    if [ -z "$QOL_TAG" ] || [ "$QOL_TAG" = latest ]; then url="$api/latest"; else url="$api/tags/$QOL_TAG"; fi

    if curl -fsSL -H 'Accept: application/vnd.github+json' -o "$QDL/release.json" "$url" 2>/dev/null; then
        need jq
        QOL_TAG_RESOLVED="$(jq -r '.tag_name // empty' "$QDL/release.json")"
        [ -n "$QOL_TAG_RESOLVED" ] || die "release has no tag name"
        mapfile -t QOL_AVAIL < <(jq -r '.assets[].name | select(endswith(".so")) | rtrimstr(".so")' "$QDL/release.json")
    else
        # The API is not reachable: the tag and plugin names must be given.
        [ -n "$QOL_TAG" ] && [ "$QOL_TAG" != latest ] || die "cannot reach the GitHub API: give the tag with --qol-version"
        [ -n "$QOL_PLUGINS" ] && [ "$QOL_PLUGINS" != all ] || die "cannot reach the GitHub API: give the plugin names with --qol-plugins"
        QOL_TAG_RESOLVED="$QOL_TAG"
        IFS=',' read -ra QOL_AVAIL <<< "$QOL_PLUGINS"
    fi

    QOL_BASE="https://github.com/$QOL_REPO/releases/download/$QOL_TAG_RESOLVED"
    fetch_qol VERSION "$QDL/VERSION" 1
    fetch_qol SHA256SUMS "$QDL/SHA256SUMS" 0 || true
}

qol_select() {
    local n
    [ "${#QOL_AVAIL[@]}" -gt 0 ] || die "no plugin (.so) files found for $QOL_SOURCE $QOL_TAG_RESOLVED"
    for n in "${QOL_AVAIL[@]}"; do
        [[ "$n" =~ ^[A-Za-z0-9_-]+$ ]] || die "refusing plugin name: $n"
    done

    if [ -n "$QOL_PLUGINS" ] && [ "$QOL_PLUGINS" != all ]; then
        local want w found
        IFS=',' read -ra want <<< "$QOL_PLUGINS"
        for w in "${want[@]}"; do
            w="${w// /}"
            found=0
            for n in "${QOL_AVAIL[@]}"; do [ "$n" = "$w" ] && found=1; done
            [ "$found" = 1 ] || die "$w is not a plugin in $QOL_SOURCE $QOL_TAG_RESOLVED"
            SELECTED+=("$w")
        done
    elif [ "$QOL_PLUGINS" = all ] || ! interactive; then
        SELECTED=("${QOL_AVAIL[@]}")
    else
        log "NeoNanoQOL $QOL_TAG_RESOLVED plugins:"
        local i=1
        for n in "${QOL_AVAIL[@]}"; do printf '  %d) %s\n' "$i" "$n"; i=$((i + 1)); done
        local reply pick
        read -r -p "Which ones? Numbers separated by spaces, or Enter for all: " reply || reply=""
        if [ -z "${reply// /}" ]; then
            SELECTED=("${QOL_AVAIL[@]}")
        else
            for pick in $reply; do
                [[ "$pick" =~ ^[0-9]+$ ]] && [ "$pick" -ge 1 ] && [ "$pick" -le "${#QOL_AVAIL[@]}" ] \
                    || die "not a number from the list: $pick"
                SELECTED+=("${QOL_AVAIL[$((pick - 1))]}")
            done
        fi
    fi

}

# Download the selected plugins, then check them against SHA256SUMS.
qol_fetch_and_verify() {
    local n
    for n in "${SELECTED[@]}"; do fetch_qol "$n.so" "$QDL/$n.so" 1; done

    local sums="$QDL/SHA256SUMS"
    if [ -f "$sums" ]; then
        local f
        for f in VERSION "${SELECTED[@]/%/.so}"; do
            awk -v f="$f" '$2 == f || $2 == "*" f { ok = 1 } END { exit !ok }' "$sums" \
                || die "$f is not listed in SHA256SUMS: refusing to install it"
        done
        if ! (cd "$QDL" && sha256sum -c --ignore-missing "$sums"); then
            die "checksum mismatch: nothing was installed"
        fi
        log "checksums OK (SHA256SUMS from $QOL_SOURCE)"
    else
        [ "$ACCEPT_UNVERIFIED" = 1 ] || die "$QOL_SOURCE $QOL_TAG_RESOLVED has no SHA256SUMS, so the files cannot be checked. Add --accept-unverified to install them anyway."
        warn "no SHA256SUMS in the release: the files are NOT verified. Their checksums:"
        (cd "$QDL" && sha256sum VERSION "${SELECTED[@]/%/.so}")
    fi
}

qol_install() {
    local n
    for n in "${SELECTED[@]}"; do
        place_file "$QDL/$n.so" "$PLUGIN_DIR/$n.so" "$n.so" 644
        printf 'qol_plugin=%s\n' "$n" >> "$TMP/qol_record"
    done
    if [ -n "$QOL_DIR" ]; then
        printf 'qol_source=dir:%s\n' "$QOL_DIR" >> "$TMP/qol_record"
    else
        printf 'qol_repo=%s\n' "$QOL_REPO" >> "$TMP/qol_record"
    fi
    printf 'qol_tag=%s\n' "$QOL_TAG_RESOLVED" >> "$TMP/qol_record"
    enable_plugins "${SELECTED[@]}"
}

# ---- neonano ----------------------------------------------------------------
check_source() {
    [ -f "$SRC_DIR/Makefile" ] && [ -f "$SRC_DIR/src/main.c" ] && [ -f "$SRC_DIR/VERSION" ] \
        || die "run install.sh from the neonano source tree"
    NN_VERSION="$(tr -d '[:space:]' < "$SRC_DIR/VERSION")"
}

check_system_write() {
    [ "$MODE" = system ] && [ "$DRY" = 0 ] && [ "$(id -u)" != 0 ] || return 0
    local probe="$SYS_PREFIX"
    while [ ! -e "$probe" ]; do probe="$(dirname "$probe")"; done
    [ -w "$probe" ] || die "a system install needs root (run with sudo) or a writable --prefix"
}

build_neonano() {
    step "Building neonano $NN_VERSION"
    run make -C "$SRC_DIR" clean
    run make -C "$SRC_DIR" PREFIX="$SYS_PREFIX" PLUGINDIR="$BUILD_PLUGINDIR"
}

install_neonano() {
    step "Installing neonano $NN_VERSION"
    run install -d "$BIN_DIR" "$PLUGIN_DIR"
    place_file "$SRC_DIR/nnano" "$BIN_DIR/nnano" nnano 755
}

# ---- actions ----------------------------------------------------------------
check_tools() {
    local t
    for t in make cc install sed awk grep cut tr mktemp; do need "$t"; done
    if [ "$QOL_CHOICE" = yes ]; then
        for t in curl sha256sum; do need "$t"; done
    fi
    if [ "$MODE" = system ]; then need getent; fi
}

install_flow() {
    check_tools
    check_source
    check_system_write

    if [ "$QOL_CHOICE" = ask ]; then
        if ask_yes "Also install the NeoNanoQOL plugins?" n; then QOL_CHOICE=yes; else QOL_CHOICE=no; fi
    fi

    # Fetch and check the QOL files first, so a bad download stops before any build.
    if [ "$QOL_CHOICE" = yes ]; then
        step "Looking up NeoNanoQOL"
        qol_discover
        qol_select
        qol_fetch_and_verify
    fi

    build_neonano
    install_neonano
    if [ "$QOL_CHOICE" = yes ]; then
        step "Installing NeoNanoQOL $QOL_TAG_RESOLVED"
        qol_install
    fi
    write_manifest

    step "Done"
    log "neonano $NN_VERSION: $BIN_DIR/nnano"
    log "plugins: $PLUGIN_DIR"
    if [ "$QOL_CHOICE" = yes ]; then
        log "NeoNanoQOL $QOL_TAG_RESOLVED enabled in $CFG: ${SELECTED[*]}"
    fi
    case ":$PATH:" in *":$BIN_DIR:"*) ;; *) log "note: $BIN_DIR is not on your PATH" ;; esac
    [ "$DRY" = 1 ] && log "(dry run: nothing was changed)"
    return 0
}

update_flow() {
    [ -f "$MANIFEST" ] || die "nothing to update: no install record at $MANIFEST"
    check_source

    step "Updating the source"
    if git -C "$SRC_DIR" rev-parse --git-dir >/dev/null 2>&1; then
        run git -C "$SRC_DIR" pull --ff-only || warn "git pull failed: building the source as it is"
    else
        log "not a git checkout: building the source as it is"
    fi

    # Keep the settings of the last install unless they were given again.
    if [ "$QOL_CHOICE" = ask ]; then
        if [ -n "$(manifest_all qol_plugin)" ]; then QOL_CHOICE=yes; else QOL_CHOICE=no; fi
    fi
    if [ "$QOL_CHOICE" = yes ]; then
        local src_rec repo_rec
        src_rec="$(manifest_all qol_source | head -n1)"
        repo_rec="$(manifest_all qol_repo | head -n1)"
        if [ -z "$QOL_DIR" ] && [ -n "$src_rec" ]; then QOL_DIR="${src_rec#dir:}"; fi
        if [ -z "$QOL_DIR" ] && [ -n "$repo_rec" ]; then QOL_REPO="$repo_rec"; fi
        if [ -z "$QOL_PLUGINS" ]; then
            QOL_PLUGINS="$(manifest_all qol_plugin | paste -sd, -)"
        fi
    fi
    install_flow
}

uninstall_flow() {
    if [ ! -f "$MANIFEST" ]; then
        [ "$PURGE" = 1 ] || die "nothing to uninstall: no install record at $MANIFEST"
        warn "no install record at $MANIFEST: only the settings are removed"
    fi

    local recorded_cfg
    if [ -f "$MANIFEST" ]; then
        recorded_cfg="$(manifest_all config | head -n1)"
        [ -n "$recorded_cfg" ] && CFG="$recorded_cfg"
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

    local names=()
    mapfile -t names < <(manifest_all enabled)
    if [ "${#names[@]}" -gt 0 ] && [ -f "$CFG" ]; then
        disable_plugins "${names[@]}"
    fi

    run rmdir "$BACKUP_DIR" 2>/dev/null || true
    run rmdir "$PLUGIN_DIR" 2>/dev/null || true
    [ -f "$MANIFEST" ] && run rm -f "$MANIFEST" || true

    if [ "$PURGE" = 1 ]; then
        step "Purging settings"
        if confirm "Delete $CONFIG_DIR (config.toml, themes and keys) and $STATE_DIR?"; then
            [ -n "$CONFIG_DIR" ] && run rm -rf "$CONFIG_DIR"
            [ -n "$STATE_DIR" ] && run rm -rf "$STATE_DIR"
        else
            log "kept $CONFIG_DIR and $STATE_DIR (add --yes to delete them without asking)"
        fi
    fi
    step "Done"
    [ "$DRY" = 1 ] && log "(dry run: nothing was changed)"
    return 0
}

setup_paths
case "$ACTION" in
    install)   install_flow ;;
    update)    update_flow ;;
    uninstall) uninstall_flow ;;
esac
