#!/bin/sh
#
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

########## Global variable definitions

# Ensure that <your CPU clock> / $SAMPLE_EVERY_N_CYCLES < $MAXIMUM_SAMPLE_RATE.
MAXIMUM_SAMPLE_RATE=10000000
SAMPLE_EVERY_N_CYCLES=10000
SAMPLE_RATE_CONFIG_FILE="/proc/sys/kernel/perf_event_max_sample_rate"
KERNEL_MAP_CONFIG_FILE="/proc/sys/kernel/kptr_restrict"

########## Usage

usage() {
cat << EOF
usage: $0 <benchmark_command>

Executes <benchmark_command> under observation by the kernel's "perf" \
framework, then calls the low level tick processor to analyze the results.
EOF
}

if [ $# -eq 0 ] || [ "$1" == "-h" ]  || [ "$1" == "--help" ] ; then
  usage
  exit 1
fi

########## Actual script execution

ACTUAL_SAMPLE_RATE=$(cat $SAMPLE_RATE_CONFIG_FILE)
if [ "$ACTUAL_SAMPLE_RATE" -lt "$MAXIMUM_SAMPLE_RATE" ] ; then
  echo "Setting appropriate maximum sample rate..."
  echo $MAXIMUM_SAMPLE_RATE | sudo tee $SAMPLE_RATE_CONFIG_FILE
fi

ACTUAL_KERNEL_MAP_RESTRICTION=$(cat $KERNEL_MAP_CONFIG_FILE)
if [ "$ACTUAL_KERNEL_MAP_RESTRICTION" -ne "0" ] ; then
  echo "Disabling kernel address map restriction..."
  echo 0 | sudo tee $KERNEL_MAP_CONFIG_FILE
fi

echo "Running benchmark..."
perf record -R -e cycles -c $SAMPLE_EVERY_N_CYCLES -f -i $@ --ll-prof
