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

NEW_VERSION=$(git rev-parse --short=7 FETCH_HEAD)

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

echo "All done!"
echo ""
echo "Make sure to update the deps/zlib/zlib.gyp if any significant changes have occurred upstream"
echo ""
echo "Please git add zlib, commit the new version:"
echo ""
echo "$ git add -A deps/zlib"
echo "$ git commit -m \"deps: update zlib to $NEW_VERSION\""
echo ""

# The last line of the script should always print the new version,
# as we need to add it to $GITHUB_ENV variable.
echo "NEW_VERSION=$NEW_VERSION"
