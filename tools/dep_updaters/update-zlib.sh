#!/bin/sh
set -e
# Shell script to update zlib in the source tree to the most recent version.
# Zlib rarely creates tags or releases, so we use the latest commit on the main branch.
# See: https://github.com/nodejs/node/pull/47417

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

echo "Comparing latest upstream with current revision"

git fetch https://chromium.googlesource.com/chromium/src/third_party/zlib.git HEAD

# Revert zconf.h changes before checking diff 
perl -i -pe 's|^//#include "chromeconf.h"|#include "chromeconf.h"|' "$DEPS_DIR/zlib/zconf.h"
git stash -- "$DEPS_DIR/zlib/zconf.h"

DIFF_TREE=$(git diff --diff-filter=d 'stash@{0}:deps/zlib' FETCH_HEAD)

git stash drop

if [ -z "$DIFF_TREE" ]; then
  echo "Skipped because zlib is on the latest version."
  exit 0
fi

# This is a rather arbitrary restriction. This script is assumed to run on
# Sunday, shortly after midnight UTC. This check thus prevents pulling in the
# most recent commits if any changes were made on Friday or Saturday (UTC).
# We don't want to pull in a commit that was just pushed, and instead rather
# wait for the next week's update. If no commits have been pushed in the last
# two days, we assume that the most recent commit is stable enough to be
# pulled in.
LAST_CHANGE_DATE=$(git log -1 --format=%ct FETCH_HEAD)
TWO_DAYS_AGO=$(date -d 'now - 2 days' '+%s')

if [ "$LAST_CHANGE_DATE" -gt "$TWO_DAYS_AGO" ]; then
  echo "Skipped because the latest version is too recent."
  exit 0
fi

LATEST_COMMIT=$(git rev-parse --short=7 FETCH_HEAD)

echo "Making temporary workspace..."

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')

cd "$WORKSPACE"

mkdir zlib

ZLIB_TARBALL="zlib-v$NEW_VERSION.tar.gz"

echo "Fetching zlib source archive"
curl -sL -o "$ZLIB_TARBALL" https://chromium.googlesource.com/chromium/src/+archive/refs/heads/main/third_party/zlib.tar.gz

log_and_verify_sha256sum "zlib" "$ZLIB_TARBALL"

gzip -dc "$ZLIB_TARBALL" | tar xf - -C zlib/

rm "$ZLIB_TARBALL"

cp "$DEPS_DIR/zlib/zlib.gyp" "$DEPS_DIR/zlib/GN-scraper.py" "$DEPS_DIR/zlib/win32/zlib.def" "$DEPS_DIR"

rm -rf "$DEPS_DIR/zlib" zlib/.git

mv zlib "$DEPS_DIR/"

mv "$DEPS_DIR/zlib.gyp" "$DEPS_DIR/GN-scraper.py" "$DEPS_DIR/zlib/"

mkdir "$DEPS_DIR/zlib/win32"

mv "$DEPS_DIR/zlib.def" "$DEPS_DIR/zlib/win32"

perl -i -pe 's|^#include "chromeconf.h"|//#include "chromeconf.h"|' "$DEPS_DIR/zlib/zconf.h"

VERSION_NUMBER=$(grep "#define ZLIB_VERSION" "$DEPS_DIR/zlib/zlib.h" | sed -n "s/^.*VERSION \"\(.*\)\"/\1/p")

NEW_VERSION="$VERSION_NUMBER-$LATEST_COMMIT"

# update version information in src/zlib_version.h
cat > "$ROOT/src/zlib_version.h" <<EOF
// This is an auto generated file, please do not edit.
// Refer to tools/dep_updaters/update-zlib.sh
#ifndef SRC_ZLIB_VERSION_H_
#define SRC_ZLIB_VERSION_H_
#define ZLIB_VERSION "$NEW_VERSION"
#endif  // SRC_ZLIB_VERSION_H_
EOF

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "zlib" "$NEW_VERSION" "src/zlib_version.h"
