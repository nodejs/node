#!/bin/bash


git_origin=$(git config --get remote.origin.url | sed 's/.\+[\/:]\([^\/]\+\/[^\/]\+\)$/\1/')
git_branch=$(git rev-parse --abbrev-ref HEAD)
v8ver=${1:-v8} #default v8
svn_prefix=https://github.com
svn_path="$svn_prefix/$git_origin/branches/$git_branch/deps/$v8ver"
#svn_path="$git_origin/branches/$git_branch/deps/$v8ver"
gclient_string="solutions = [{'name': 'v8', 'url': '$svn_path', 'managed': False}]"

# clean up if someone presses ctrl-c
trap cleanup INT

function cleanup() {
  trap - INT

  rm .gclient || true
  rm .gclient_entries || true
  rm -rf _bad_scm/ || true

  #if v8ver isn't v8, move the v8 folders
  #back to what they were
  if [ "$v8ver" != "v8" ]; then
    mv v8 $v8ver
    mv .v8old v8
  fi
  exit 0
}

cd deps
echo $gclient_string > .gclient
if [ "$v8ver" != "v8" ]; then
  mv v8 .v8old
  mv $v8ver v8
fi
gclient sync
cleanup
