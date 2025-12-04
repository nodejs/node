#!/bin/sh

CPUPATH=/sys/devices/system/cpu

MAXID=$(cat $CPUPATH/present | awk -F- '{print $NF}')

[ "$(uname -s || true)" = "Linux" ] || \
  echo "Warning: This script supports Linux only." >&2

[ "$(id -u || true)" = "0" ] || \
  echo "Warning: This script typically needs root access to modify CPU governor. Consider running it with sudo." >&2

get_default_governor() {
  case "$(cat "$CPUPATH/cpu0/cpufreq/scaling_available_governors")" in
    *"schedutil"*)    echo "schedutil"   ;;
    *"ondemand"*)     echo "ondemand"    ;;
    *"conservative"*) echo "conservative";;
    *)                echo "powersave"   ;;
  esac

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
