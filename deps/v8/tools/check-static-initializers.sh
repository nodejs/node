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

# Checks that the number of compilation units having at least one static
# initializer in d8 matches the one defined below.

# Allow:
#  - _GLOBAL__I__ZN2v810LineEditor6first_E
#  - _GLOBAL__I__ZN2v88internal32AtomicOps_Internalx86CPUFeaturesE
expected_static_init_count=2

v8_root=$(readlink -f $(dirname $BASH_SOURCE)/../)

if [ -n "$1" ] ; then
  d8="${v8_root}/$1"
else
  d8="${v8_root}/d8"
fi

if [ ! -f "$d8" ]; then
  echo "d8 binary not found: $d8"
  exit 1
fi

static_inits=$(nm "$d8" | grep _GLOBAL_ | grep _I_ | awk '{ print $NF; }')

static_init_count=$(echo "$static_inits" | wc -l)

if [ $static_init_count -gt $expected_static_init_count ]; then
  echo "Too many static initializers."
  echo "$static_inits"
  exit 1
else
  echo "Static initializer check passed ($static_init_count initializers)."
  exit 0
fi
