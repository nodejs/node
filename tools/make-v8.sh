#!/bin/bash

if [ $# -eq 0 ]; then
  # Default branch is master
  BRANCH="master"
else
  # eg: 5.4-lkgr
  BRANCH="$1"
fi

# clean up if someone presses ctrl-c
trap cleanup INT

function cleanup() {
  trap - INT
  rm .gclient || true
  rm .gclient_entries || true
  rm -rf _bad_scm/ || true
  rm -rf .v8old
  if [ "$BRANCH" == "master" ]; then
    echo "git cleanup if branch is master"
    git ls-files -m | xargs git checkout --
    git clean -fd >/dev/null
  fi
  exit 0
}

cd deps
mv v8 .v8old

echo "Fetching v8 from chromium.googlesource.com"
fetch v8
if [ "$?" -ne 0 ]; then
  echo "V8 fetch failed"
  exit 1
fi
echo "V8 fetched"

cd v8

echo "Checking out branch:$BRANCH"
if [ "$BRANCH" != "master" ]; then
   git fetch
   git checkout origin/$BRANCH
fi

echo "Sync dependencies"
gclient sync

cd ..
cleanup
