#!/bin/sh

CPUPATH=/sys/devices/system/cpu

MAXID=$(cat $CPUPATH/present | awk -F- '{print $NF}')

if [ "$(uname -s || true)" != "Linux" ]; then
  echo "Error: This script runs on Linux only." >&2
  exit 1
fi

if [ "$(id -u || true)" -ne 0 ]; then
  echo "Error: Run as root (sudo) to modify CPU governor." >&2
  exit 1
fi

get_default_governor() {
  available_governors=$(cat "$CPUPATH/cpu0/cpufreq/scaling_available_governors")

  if echo "$available_governors" | grep -q "schedutil"; then
    echo "schedutil"
  elif echo "$available_governors" | grep -q "ondemand"; then
    echo "ondemand"
  elif echo "$available_governors" | grep -q "conservative"; then
    echo "conservative"
  else
    echo "powersave"
  fi
}

set_governor() {
  governor_name="$1"

  echo "Setting governor for all CPU cores to \"$governor_name\"..."

  i=0
  while [ "$i" -le "$MAXID" ]; do
    echo "$1" > "$CPUPATH/cpu$i/cpufreq/scaling_governor"
    i=$((i + 1))
  done

  echo "Done."
}

usage() {
  default_gov=$(get_default_governor)
  echo "CPU Governor Management Script"
  echo "----------------------------------------------------------------------------"
  echo "Usage: $0 [command]"
  echo
  echo "Commands:"
  echo "  fast      Sets the governor to 'performance' for maximum speed."
  echo "           (Warning: Increases heat/power use. Use for short-term tasks.)"
  echo
  echo "  reset     Resets the governor to the system's recommended default ('$default_gov')."
  echo
  echo "  get       Shows the current CPU governor for ALL cores."
  echo "----------------------------------------------------------------------------"
}

case "$1" in
  fast | performance)
    echo "Warning: The 'performance' mode locks the CPU at its highest speed."
    echo "It is highly recommended to 'reset' after your task is complete."
    set_governor "performance"
    ;;

  reset | default)
    default_governor=$(get_default_governor)
    set_governor "$default_governor"
    ;;

  get | status)
    echo "Current governor status for all cores:"
    grep . "$CPUPATH"/cpu*/cpufreq/scaling_governor
    ;;

  *)
    usage
    exit 1
    ;;
esac

exit 0
