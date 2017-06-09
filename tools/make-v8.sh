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
  find v8 -name ".git" | xargs rm -rf || true
  echo "git cleanup"
  git reset --hard HEAD
  git clean -fdq
  # unstash local changes
  git stash pop
  exit 0
}

cd deps
# stash local changes
git stash
rm -rf v8

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
