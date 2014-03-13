#!/bin/sh
#
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

if [ "$#" -lt 1 ]; then
  echo "Usage: tools/cross_build_gcc.sh <GCC prefix> [make arguments ...]"
  exit 1
fi

export CXX=$1g++
export AR=$1ar
export RANLIB=$1ranlib
export CC=$1gcc
export LD=$1g++

OK=1
if [ ! -x "$CXX" ]; then
  echo "Error: $CXX does not exist or is not executable."
  OK=0
fi
if [ ! -x "$AR" ]; then
  echo "Error: $AR does not exist or is not executable."
  OK=0
fi
if [ ! -x "$RANLIB" ]; then
  echo "Error: $RANLIB does not exist or is not executable."
  OK=0
fi
if [ ! -x "$CC" ]; then
  echo "Error: $CC does not exist or is not executable."
  OK=0
fi
if [ ! -x "$LD" ]; then
  echo "Error: $LD does not exist or is not executable."
  OK=0
fi
if [ $OK -ne 1 ]; then
  exit 1
fi

shift
make snapshot=off $@
