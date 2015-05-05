#!/bin/bash
# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


########## Global variable definitions

BASE_URL="https://code.google.com/p/v8/source/list"
VERSION="include/v8-version.h"
MAJOR="V8_MAJOR_VERSION"
MINOR="V8_MINOR_VERSION"
BUILD="V8_BUILD_NUMBER"
PATCH="V8_PATCH_LEVEL"

V8="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

########## Function definitions

cd $V8

usage() {
cat << EOF
usage: $0 OPTIONS

Fetches V8 revision information from a git-svn checkout.

OPTIONS:
  -h    Show this message.

  -i    Print revision info for all branches matching the V8 version.
          Example usage: $0 -i 3.19.10$
          Output format: [Git hash] [SVN revision] [V8 version]

  -v    Print the V8 version tag for a trunk SVN revision.
          Example usage: $0 -v 14981
          Output format: [V8 version]

  -m    Print all patches that were merged to the specified V8 branch.
          Example usage: $0 -m 3.18
          Output format: [V8 version] [SVN revision] [SVN patch merged]*.

  -p    Print all patches merged to a specific V8 point-release.
          Example usage: $0 -p 3.19.12.1
          Output format: [SVN patch merged]*

  -u    Print a link to all SVN revisions between two V8 revision tags.
          Example usage: $0 -u 3.19.10:3.19.11
EOF
}

tags() {
  git for-each-ref --format="%(objectname) %(refname:short)" refs/remotes/svn
}

tag_revision() {
  cut -d" " -f1
}

tag_log() {
  git log --format="%h %ci %ce %s" -1 $1
}

v8_hash() {
  tags | grep "svn/tags/$1$" | tag_revision
}

point_merges() {
  echo $1 | grep -o "r[0-9]\+"
}

hash_to_svn() {
  git svn log -1 --oneline $1 | cut -d" " -f1
}

tag_version() {
  tags | grep svn/tags/$1 | while read tag; do
    id=$(echo $tag | grep -o "[^/]*$")
    rev=$(echo $tag | tag_revision)
    svn=$(hash_to_svn $rev)
    echo $rev $svn $id
  done
}

svn_rev() {
  git svn find-rev $2 svn/$1
}

v8_rev() {
  cd $(git rev-parse --show-toplevel)
  rev=$(git show $1:$VERSION \
      | grep "#define" \
      | grep "$MAJOR\|$MINOR\|$BUILD\|$PATCH" \
      | grep -o "[0-9]\+$" \
      | tr "\\n" ".")
  echo ${rev%?}
}

merges_to_branch() {
  git cherry -v svn/trunk svn/$1 | while read merge; do
    h=$(echo $merge | cut -d" " -f2)
    svn=$(svn_rev $1 $h)
    merges=$(echo $merge | grep -o "r[0-9]\+")
    rev=$(v8_rev $h)
    echo $rev r$svn $merges
  done
}

url_for() {
  first=$(svn_rev trunk $(v8_hash $(echo $1 | cut -d":" -f1)))
  last=$(svn_rev trunk $(v8_hash $(echo $1 | cut -d":" -f2)))
  num=$[ $last - $first]
  echo "$BASE_URL?num=$num&start=$last"
}

########## Option parsing

while getopts ":hi:v:m:p:u:" OPTION ; do
  case $OPTION in
    h)  usage
        exit 0
        ;;
    i)  tag_version $OPTARG
        ;;
    v)  v8_rev $(svn_rev trunk r$OPTARG)
        ;;
    m)  merges_to_branch $OPTARG
        ;;
    p)  echo $(point_merges "$(tag_log $(v8_hash $OPTARG)^1)")
        ;;
    u)  url_for $OPTARG
        ;;
    ?)  echo "Illegal option: -$OPTARG"
        usage
        exit 1
        ;;
  esac
done
