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

dual_core() {
  echo "Switching to dual-core mode"
  for (( i=2; i<=$MAXID; i++ )); do
    echo 0 > $CPUPATH/cpu$i/online
  done
}

single_core() {
  echo "Switching to single-core mode"
  for (( i=1; i<=$MAXID; i++ )); do
    echo 0 > $CPUPATH/cpu$i/online
  done
}


all_cores() {
  echo "Reactivating all CPU cores"
  for (( i=2; i<=$MAXID; i++ )); do
    echo 1 > $CPUPATH/cpu$i/online
  done
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
  *)
    echo "Usage: $0 fast|slow|default|singlecore|dualcore|all"
    exit 1
    ;;
esac 
