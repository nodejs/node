#! /bin/sh
#
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

########## Global variable definitions

# Ensure that <your CPU clock> / $SAMPLE_EVERY_N_CYCLES < $MAXIMUM_SAMPLE_RATE.
MAXIMUM_SAMPLE_RATE=10000000
SAMPLE_EVERY_N_CYCLES=10000
SAMPLE_RATE_CONFIG_FILE="/proc/sys/kernel/perf_event_max_sample_rate"
KERNEL_MAP_CONFIG_FILE="/proc/sys/kernel/kptr_restrict"
CALL_GRAPH_METHOD="fp"  # dwarf does not play nice with JITted objects.
EVENT_TYPE=${EVENT_TYPE:=cycles:u}

########## Usage

usage() {
cat << EOF
usage: $0 <benchmark_command>

Executes <benchmark_command> under observation by Linux perf.
Sampling event is cycles in user space, call graphs are recorded.
EOF
}

if [ $# -eq 0 ] || [ "$1" = "-h" ]  || [ "$1" = "--help" ] ; then
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

# Extract the command being perfed, so that we can prepend arguments to the
# arguments that the user supplied.
COMMAND=$1
shift 1

echo "Running..."
perf record -R \
  -e $EVENT_TYPE \
  -c $SAMPLE_EVERY_N_CYCLES \
  --call-graph $CALL_GRAPH_METHOD \
  -i "$COMMAND" --perf_basic_prof "$@"
