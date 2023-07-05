#!/bin/sh
set -e
# Shell script to update GoogleTest in the source tree to the most recent version.
# GoogleTest follows the Abseil Live at Head philosophy and rarely creates tags
# or GitHub releases, so instead, we use the latest commit on the main branch.

BASE_DIR=$(cd "$(dirname "$0")/../.." && pwd)
DEPS_DIR="$BASE_DIR/deps"

NEW_UPSTREAM_SHA1=$(git ls-remote "https://github.com/google/googletest.git" HEAD | awk '{print $1}')
NEW_VERSION=$(echo "$NEW_UPSTREAM_SHA1" | head -c 7)

# shellcheck disable=SC1091
. "$BASE_DIR/tools/dep_updaters/utils.sh"

echo "Comparing $NEW_VERSION with current revision"

git remote add googletest-upstream https://github.com/google/googletest.git
git fetch googletest-upstream "$NEW_UPSTREAM_SHA1"
git remote remove googletest-upstream

DIFF_TREE=$(
  git diff HEAD:deps/googletest/LICENSE "$NEW_UPSTREAM_SHA1:LICENSE"
  git diff-tree HEAD:deps/googletest/include "$NEW_UPSTREAM_SHA1:googletest/include"
  git diff-tree HEAD:deps/googletest/src "$NEW_UPSTREAM_SHA1:googletest/src"
)

if [ -z "$DIFF_TREE" ]; then
  echo "Skipped because googletest is on the latest version."
  exit 0
fi

# This is a rather arbitrary restriction. This script is assumed to run on
# Sunday, shortly after midnight UTC. This check thus prevents pulling in the
# most recent commits if any changes were made on Friday or Saturday (UTC).
# Because of Google's own "Live at Head" philosophy, new bugs that are likely to
# affect Node.js tend to be fixed quickly, so we don't want to pull in a commit
# that was just pushed, and instead rather wait for the next week's update. If
# no commits have been pushed in the last two days, we assume that the most
# recent commit is stable enough to be pulled in.
LAST_CHANGE_DATE=$(git log -1 --format=%ct "$NEW_UPSTREAM_SHA1" -- LICENSE googletest/include googletest/src)
TWO_DAYS_AGO=$(date -d 'now - 2 days' '+%s')
if [ "$LAST_CHANGE_DATE" -gt "$TWO_DAYS_AGO" ]; then
  echo "Skipped because the latest version is too recent."
  exit 0
fi

echo "Creating temporary work tree"

WORKSPACE=$(mktemp -d 2> /dev/null || mktemp -d -t 'tmp')
WORKTREE="$WORKSPACE/googletest"

cleanup () {
  EXIT_CODE=$?
  [ -d "$WORKTREE" ] && git worktree remove -f "$WORKTREE"
  [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
  exit $EXIT_CODE
}

trap cleanup INT TERM EXIT

git worktree add "$WORKTREE" "$NEW_UPSTREAM_SHA1"

echo "Copying LICENSE, include and src to deps/googletest"
for p in LICENSE googletest/include googletest/src ; do
  rm -rf "$DEPS_DIR/googletest/$(basename "$p")"
  cp -R "$WORKTREE/$p" "$DEPS_DIR/googletest/$(basename "$p")"
done

echo "Updating googletest.gyp"

NEW_GYP=$(
  sed "/'googletest_sources': \[/q" "$DEPS_DIR/googletest/googletest.gyp"
  for f in $( (cd deps/googletest/ && find include src -type f \( -iname '*.h' -o -iname '*.cc' \) ) | LANG=C LC_ALL=C sort --stable ); do
    if [ "$(basename "$f")" != "gtest_main.cc" ] &&
       [ "$(basename "$f")" != "gtest-all.cc" ] &&
       [ "$(basename "$f")" != "gtest_prod.h" ] ; then
      echo "      '$f',"
    fi
  done
  sed -ne '/\]/,$ p' "$DEPS_DIR/googletest/googletest.gyp"
)

echo "$NEW_GYP" >"$DEPS_DIR/googletest/googletest.gyp"

# Update the version number on maintaining-dependencies.md
# and print the new version as the last line of the script as we need
# to add it to $GITHUB_ENV variable
finalize_version_update "googletest" "$NEW_VERSION"
