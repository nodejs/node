#!/bin/bash

# Get V8 branch from v8/include/v8-version.h
MAJOR=$(grep V8_MAJOR_VERSION deps/v8/include/v8-version.h | cut -d ' ' -f 3)
MINOR=$(grep V8_MINOR_VERSION deps/v8/include/v8-version.h | cut -d ' ' -f 3)
BRANCH=$MAJOR.$MINOR

# clean up if someone presses ctrl-c
trap cleanup INT

function cleanup() {
  trap - INT
  rm .gclient || true
  rm .gclient_entries || true
  rm -rf _bad_scm/ || true
  echo "git cleanup"
  git reset --hard HEAD
  git clean -e .v8old -ffdq
  # Copy local files
  rsync -a .v8old/ v8/
  rm -rf .v8old
  exit 0
}

cd deps
# Preserve local changes
mv v8 .v8old

echo "Fetching V8 from chromium.googlesource.com"
fetch v8
if [ "$?" -ne 0 ]; then
  echo "V8 fetch failed"
  exit 1
fi
echo "V8 fetched"

cd v8

echo "Checking out branch:$BRANCH"
git checkout remotes/branch-heads/$BRANCH

echo "Sync dependencies"
gclient sync

cd ..
cleanup
