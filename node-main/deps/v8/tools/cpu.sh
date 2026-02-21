#!/bin/bash
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CPUPATH=/sys/devices/system/cpu

MAXID=$(cat $CPUPATH/present | awk -F- '{print $NF}')

set_governor() {
  echo "Setting CPU frequency governor to \"$1\""
  for (( i=0; i<=$MAXID; i++ )); do
    echo "$1" > $CPUPATH/cpu$i/cpufreq/scaling_governor
  done
}

enable_cores() {
  # $1: How many cores to enable.
  for (( i=1; i<=$MAXID; i++ )); do
    if [ "$i" -lt "$1" ]; then
      echo 1 > $CPUPATH/cpu$i/online
    else
      echo 0 > $CPUPATH/cpu$i/online
    fi
  done
}

dual_core() {
  echo "Switching to dual-core mode"
  enable_cores 2
}

single_core() {
  echo "Switching to single-core mode"
  enable_cores 1
}


all_cores() {
  echo "Reactivating all CPU cores"
  enable_cores $((MAXID+1))
}


limit_cores() {
  # $1: How many cores to enable.
  echo "Limiting to $1 cores"
  enable_cores $1
}

case "$1" in
  fast | performance)
    set_governor "performance"
    ;;
  slow | powersave)
    set_governor "powersave"
    ;;
  default | ondemand)
    set_governor "ondemand"
    ;;
  dualcore | dual)
    dual_core
    ;;
  singlecore | single)
    single_core
    ;;
  allcores | all)
    all_cores
    ;;
  limit_cores)
    if [ $# -ne 2 ]; then
      echo "Usage $0 limit_cores <num>"
      exit 1
    fi
    limit_cores $2
    ;;
  *)
    echo "Usage: $0 fast|slow|default|singlecore|dualcore|all|limit_cores"
    exit 1
    ;;
esac 
