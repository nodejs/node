#!/bin/bash

CPUPATH=/sys/devices/system/cpu

MAXID=$(cat $CPUPATH/present | awk -F- '{print $NF}')

set_governor() {
  echo "Setting CPU frequency governor to \"$1\""
  for (( i=0; i<=$MAXID; i++ )); do
    echo "$1" > $CPUPATH/cpu$i/cpufreq/scaling_governor
  done
}

case "$1" in
  fast | performance)
    set_governor "performance"
    ;;
    echo "Usage: $0 fast"
    exit 1
    ;;
esac
