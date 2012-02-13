#!/bin/bash
# Copyright 2012 the V8 project authors. All rights reserved.
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

# Inspired by and based on:
# http://src.chromium.org/viewvc/chrome/trunk/src/tools/bash-completion

# Flag completion rule for bash.
# To load in your shell, "source path/to/this/file".

v8_source=$(readlink -f $(dirname $BASH_SOURCE)/..)

_v8_flag() {
  local cur defines targets
  cur="${COMP_WORDS[COMP_CWORD]}"
  defines=$(cat src/flag-definitions.h \
    | grep "^DEFINE" \
    | grep -v "DEFINE_implication" \
    | sed -e 's/_/-/g')
  targets=$(echo "$defines" \
    | sed -ne 's/^DEFINE-[^(]*(\([^,]*\).*/--\1/p'; \
    echo "$defines" \
    | sed -ne 's/^DEFINE-bool(\([^,]*\).*/--no\1/p'; \
    cat src/d8.cc \
    | grep "strcmp(argv\[i\]" \
    | sed -ne 's/^[^"]*"--\([^"]*\)".*/--\1/p')
  COMPREPLY=($(compgen -W "$targets" -- "$cur"))
  return 0
}

complete -F _v8_flag -f d8
