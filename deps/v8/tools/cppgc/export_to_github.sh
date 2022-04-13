#!/bin/sh
#
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

source_dir=$(cd "$(dirname "$0")"; pwd -P)

copybara_exe=copybara
copybara_file="$source_dir/copy.bara.sky"
init_history=''

for arg in "$@"; do
  case $arg in
    --copybara-exe=*)
      copybara_exe="${arg#*=}"
      shift
      ;;
    --copybara-file=*)
      copybara_file="${arg#*=}"
      shift
      ;;
    --init-history)
      init_history='--init-history --force'
      shift
      ;;
    *)
      echo -e "Usage:$arg"
      echo -e "    export_to_github.sh [--copybara-exe=<path-to-copybara>]\n" \
              "                       [--copybara-file=<path-to-copy.bara.sky>]"
      exit 1
  esac
done

v8_origin="https://chromium.googlesource.com/v8/v8.git"
v8_ref="master"

NOCOLOR="\033[0m"
RED="\033[0;31m"
GREEN="\033[0;32m"
BLUE="\033[0;34m"

function fail {
  echo -e "${RED}${1}${NOCOLOR}" > /dev/stderr
  exit 1
}

function success {
  echo -e "${BLUE}${1}${NOCOLOR}" > /dev/stderr
  exit 0
}

function message {
  echo -e "${GREEN}${1}${NOCOLOR}" > /dev/stderr
}

function cleanup {
  if [ -d "$git_temp_dir" ]; then
    rm -rf $git_temp_dir
  fi
}

trap "exit 1" HUP INT PIPE QUIT TERM
trap cleanup EXIT

[ ! -x $copybara_exe ] && fail "$copybara_exe doesn't exist or was not found in PATH!"
[ ! -f $copybara_file ] && fail "Input $copybara_file doesn't exist!"

git_temp_dir=$(mktemp -d)
if [[ ! "$git_temp_dir" || ! -d "$git_temp_dir" ]]; then
  fail "Failed to create temporary dir"
fi

if [[ $init_history ]]; then
  read -p "--init-history is only supposed to be used on the first export of \
cppgc. Is this what is really intended? (y/N)" answer
  if [ "$answer" != "y" ]; then
    exit 0
  fi
fi

message "Running copybara..."
$copybara_exe $init_history $copybara_file --dry-run --git-destination-path $git_temp_dir
result=$?
if [ "$result" -eq 4 ]; then
  success "Nothing needs to be done, exiting..."
elif [ "$result" -ne 0 ]; then
  fail "Failed to run copybara"
fi

cd $git_temp_dir

main_gn="BUILD.gn"
test_gn="test/unittests/BUILD.gn"
gen_cmake="tools/cppgc/gen_cmake.py"

message "Checking out BUILD.gn files..."
git remote add v8_origin "$v8_origin"
git fetch --depth=1 v8_origin $v8_ref
git checkout v8_origin/master -- "$main_gn" "$test_gn" "$gen_cmake" \
  || fail "Failed to checkout BUILD.gn from V8 origin"

message "Generating CMakeLists.txt..."
cmakelists="$git_temp_dir/CMakeLists.txt"
$gen_cmake --out=$cmakelists --main-gn=$main_gn --test-gn=$test_gn \
  || fail "CMakeLists.txt generation has failed!"

git rm -f $main_gn $test_gn $gen_cmake > /dev/null

if git status -s | grep -q $(basename $cmakelists); then
  message "CMakeLists.txt needs to be changed"
  git add $cmakelists
  git commit --amend --no-edit > /dev/null
else
  message "No changes in CMakeLists.txt need to be done"
fi

message "Pushing changes to GitHub..."
git push copybara_remote master

success "CppGC GitHub mirror was successfully updated"
