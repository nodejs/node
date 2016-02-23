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

DEPS_STRING='"v8_revision":'
INFO=tools/v8-info.sh

V8="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

########## Function definitions

usage() {
cat << EOF
usage: $0 OPTIONS

Run in chromium/src to get information about V8 rolls.

OPTIONS:
  -h    Show this message.
  -n    Number of rolls to print information about.
  -s    Chromium git hash to start printing V8 information about.
EOF
}

v8_line() {
  git show $1:DEPS | grep -n $DEPS_STRING | cut -d":" -f1
}

v8_info() {
  git blame -L$(v8_line $1),+1 $1 DEPS | grep $DEPS_STRING
}

v8_svn() {
  sed -e 's/^.*"\([0-9]\+\)",$/\1/'
}

v8_roll() {
  cut -d" " -f1
}

find_rev() {
  git svn find-rev $1
}

msg() {
  msg=$(git log --format="%h %ci %ce" -1 $1)
  h=$(echo $msg | cut -d" " -f1)
  d=$(echo $msg | cut -d" " -f2)
  t=$(echo $msg | cut -d" " -f3)
  a=$(echo $msg | cut -d" " -f5)
  a1=$(echo $a | cut -d"@" -f1)
  a2=$(echo $a | cut -d"@" -f2)
  echo $h $d $t $a1@$a2
}

v8_revision() {
  cd $V8
  $INFO -v $1
}

rolls() {
  roll=$2
  for i in $(seq 1 $1); do
    info=$(v8_info $roll)
    roll=$(echo $info | v8_roll $roll)
    trunk=$(echo $info | v8_svn $roll)
    echo "$(v8_revision $trunk) $trunk $(find_rev $roll) $(msg $roll)"
    roll=$roll^1
  done
}

########## Option parsing

REVISIONS=1
START=HEAD

while getopts ":hn:s:" OPTION ; do
  case $OPTION in
    h)  usage
        exit 0
        ;;
    n) REVISIONS=$OPTARG
        ;;
    s) START=$OPTARG
        ;;
    ?)  echo "Illegal option: -$OPTARG"
        usage
        exit 1
        ;;
  esac
done

rolls $REVISIONS $START
