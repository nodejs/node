#!/bin/bash

# set configurations that will be "sticky" on this system,
# surviving npm self-updates.

CONFIGS=()
i=0

# get the location of this file.
unset CDPATH
CONFFILE=$(cd $(dirname "$0"); pwd -P)/npmrc

while [ $# -gt 0 ]; do
  conf="$1"
  case $conf in
    --help)
      echo "./configure --param=value ..."
      exit 0
      ;;
    --*)
      CONFIGS[$i]="${conf:2}"
      ;;
    *)
      CONFIGS[$i]="$conf"
      ;;
  esac
  let i++
  shift
done

for c in "${CONFIGS[@]}"; do
  echo "$c" >> "$CONFFILE"
done
