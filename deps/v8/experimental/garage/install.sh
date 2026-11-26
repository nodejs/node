#!/bin/sh
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Install or update garage from a GitHub release.
#
#   curl -fsSL https://raw.githubusercontent.com/victorgomes/garage/main/install.sh | sh
#
# Re-running this is the update path: it fetches the latest release, and says
# so and stops if you already have it.
#
#   sh install.sh                     # latest
#   sh install.sh --version v0.1.0    # a specific release
#   sh install.sh --bin-dir ~/bin     # somewhere other than ~/.local/bin
#   sh install.sh --force             # reinstall even if the version matches
#
# Environment equivalents: GARAGE_VERSION, GARAGE_BIN_DIR.
#
# POSIX sh on purpose — it is piped into `sh`, so it cannot assume bash. No
# root required, and nothing is installed until the published sha256 matches.

set -eu

REPO="victorgomes/garage"
DEFAULT_BIN_DIR="$HOME/.local/bin"
# Empty unless asked for: an empty BIN_DIR is what lets the script find and
# replace an existing install instead of imposing a directory.
BIN_DIR="${GARAGE_BIN_DIR:-}"
VERSION="${GARAGE_VERSION:-}"
FORCE=0
TMP=""

say() { printf '%s\n' "$*"; }
err() { printf 'error: %s\n' "$*" >&2; }

# Must end on a success status: this runs from the EXIT trap, and a trailing
# failed test here becomes the script's exit code. `[ -n "$TMP" ] && rm …`
# returns 1 whenever nothing was downloaded — which is exactly the
# already-up-to-date path, the most common run once people update by
# re-running this.
cleanup() {
  if [ -n "$TMP" ]; then
    rm -rf "$TMP"
  fi
  return 0
}
trap cleanup EXIT INT TERM

usage() {
  sed -n '3,18p' "$0" | sed 's/^# \{0,1\}//'
  exit 0
}

while [ $# -gt 0 ]; do
  case "$1" in
    --version) VERSION="${2:-}"; shift 2 ;;
    --version=*) VERSION="${1#*=}"; shift ;;
    --bin-dir) BIN_DIR="${2:-}"; shift 2 ;;
    --bin-dir=*) BIN_DIR="${1#*=}"; shift ;;
    --force) FORCE=1; shift ;;
    -h|--help) usage ;;
    *) err "unknown option: $1 (try --help)"; exit 2 ;;
  esac
done

# --- the one download primitive, so the rest of the script does not care ----
if command -v curl >/dev/null 2>&1; then
  fetch() { curl -fsSL "$1" -o "$2"; }
  fetch_stdout() { curl -fsSL "$1"; }
elif command -v wget >/dev/null 2>&1; then
  fetch() { wget -qO "$2" "$1"; }
  fetch_stdout() { wget -qO- "$1"; }
else
  err "need curl or wget"
  exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
  sha256_of() { sha256sum "$1" | cut -d' ' -f1; }
elif command -v shasum >/dev/null 2>&1; then
  sha256_of() { shasum -a 256 "$1" | cut -d' ' -f1; }
else
  err "need sha256sum or shasum to verify the download"
  exit 1
fi

# --- which build ------------------------------------------------------------
os="$(uname -s)"
arch="$(uname -m)"
case "$os" in
  Linux)
    case "$arch" in
      x86_64|amd64)   asset="linux-x86_64" ;;
      aarch64|arm64)  asset="linux-aarch64" ;;
      *) err "unsupported Linux architecture: $arch"; exit 1 ;;
    esac
    ;;
  Darwin)
    # One universal binary covers both Apple Silicon and Intel.
    asset="macos-universal"
    ;;
  *)
    err "unsupported OS: $os (garage builds for Linux and macOS)"
    exit 1
    ;;
esac

# --- which version ----------------------------------------------------------
if [ -z "$VERSION" ]; then
  say "resolving the latest release..."
  # Parsing JSON with sed rather than requiring jq: one field, fixed shape.
  VERSION="$(fetch_stdout "https://api.github.com/repos/$REPO/releases/latest" \
    | sed -n 's/.*"tag_name" *: *"\([^"]*\)".*/\1/p' | head -1)"
  if [ -z "$VERSION" ]; then
    err "could not resolve the latest release (rate-limited, or no releases yet)."
    err "pass one explicitly:  --version v0.1.0"
    exit 1
  fi
fi
case "$VERSION" in v*) ;; *) VERSION="v$VERSION" ;; esac

# --- where the binary goes --------------------------------------------------
# An update must replace the garage you already run, wherever that is. Always
# installing to a default directory would leave a second copy behind and let
# PATH order decide which one you get — an update that silently does nothing.
#
#   1. --bin-dir, when given, wins outright
#   2. otherwise replace the garage already on PATH, in place
#   3. otherwise a fresh install into ~/.local/bin
fresh=0
if [ -n "$BIN_DIR" ]; then
  existing="$BIN_DIR/garage"
  [ -e "$existing" ] || fresh=1
elif found="$(command -v garage 2>/dev/null)" && [ -n "$found" ]; then
  existing="$found"
  say "found garage at $existing — updating in place"
else
  existing="$DEFAULT_BIN_DIR/garage"
  fresh=1
fi
bin_dir="$(dirname "$existing")"

# Already current? Say so rather than redownloading — this is the common case
# once people start re-running the script to update.
if [ "$FORCE" -eq 0 ] && [ -x "$existing" ]; then
  have="$("$existing" --version 2>/dev/null | awk '{print $2}')" || have=""
  if [ -n "$have" ] && [ "v$have" = "$VERSION" ]; then
    say "garage $VERSION is already installed at $existing"
    say "(re-run with --force to reinstall)"
    exit 0
  fi
fi

# Fail on permissions now, with the path in hand, rather than after a download.
# Never escalate: if the binary lives somewhere privileged, that is the user's
# call to make, not this script's.
if [ -e "$existing" ] && [ ! -w "$existing" ]; then
  err "$existing is not writable by you."
  err "re-run with enough privileges to replace it, or pass --bin-dir to"
  err "install somewhere you own."
  exit 1
fi
if [ ! -e "$existing" ] && [ -d "$bin_dir" ] && [ ! -w "$bin_dir" ]; then
  err "$bin_dir is not writable by you. Pass --bin-dir to choose elsewhere."
  exit 1
fi

name="garage-$VERSION-$asset"
url="https://github.com/$REPO/releases/download/$VERSION/$name.tar.gz"

TMP="$(mktemp -d)"
say "downloading $name.tar.gz"
if ! fetch "$url" "$TMP/$name.tar.gz"; then
  err "download failed: $url"
  err "check that $VERSION exists and has a $asset build."
  exit 1
fi

# --- verify before anything touches the filesystem --------------------------
say "verifying checksum"
if ! fetch "$url.sha256" "$TMP/$name.tar.gz.sha256"; then
  err "could not download the checksum for $name.tar.gz"
  exit 1
fi
want="$(cut -d' ' -f1 < "$TMP/$name.tar.gz.sha256")"
got="$(sha256_of "$TMP/$name.tar.gz")"
if [ "$want" != "$got" ]; then
  err "checksum mismatch for $name.tar.gz"
  err "  published: $want"
  err "  download:  $got"
  err "refusing to install."
  exit 1
fi

# --- install ----------------------------------------------------------------
tar -xzf "$TMP/$name.tar.gz" -C "$TMP"
if [ ! -f "$TMP/$name/garage" ]; then
  err "unexpected archive layout: no $name/garage inside the tarball"
  exit 1
fi

mkdir -p "$bin_dir"
# Install to a temp name in the same directory and rename over the old binary:
# writing into a running executable fails with ETXTBSY, a rename does not, and
# it means a half-copied binary is never reachable under the real name.
install_tmp="$bin_dir/.garage.$$"
cp "$TMP/$name/garage" "$install_tmp"
chmod 755 "$install_tmp"
mv -f "$install_tmp" "$existing"

say ""
if [ "$fresh" -eq 1 ]; then
  say "garage $VERSION installed to $existing"
else
  say "garage updated to $VERSION at $existing"
fi

# --- is it actually usable? -------------------------------------------------
case ":${PATH}:" in
  *":$bin_dir:"*)
    say "run: garage --help"
    ;;
  *)
    say ""
    say "$bin_dir is not on your PATH. Add it:"
    say ""
    say "    export PATH=\"\$PATH:$bin_dir\""
    say ""
    say "or run it directly: $existing --help"
    ;;
esac
say ""
say "to update later, run this script again."
