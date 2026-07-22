#!/bin/sh

if [ "$#" -ne 3 ] || ! [ -f "$1" ]; then
  echo ===========================
  echo "Script to modify sidedeck references to a new DLL name"
  echo ===========================
  echo "Usage: $0 originalsidedeck modifiedsidedeck newdllreference" >&2
  exit 1
fi

originalsidedeck=$1
outputsidedeck=$2
newdllname=$3

SCRIPT_DIR=$(dirname "$0")
ID=`date +%C%y%m%d_%H%M%S`
TMP="/tmp/sidedeck-$(basename "$0").$ID.tmp"
TMP2="/tmp/sidedeck-$(basename "$0").$ID.tmp.2"

# Remove on exit/interrupt
trap '/bin/rm -rf "$TMP" "$TMP2" && exit' EXIT INT TERM QUIT HUP

set -x
dd conv=unblock cbs=80 if="$originalsidedeck" of="$TMP"
chtag -tc 1047 "$TMP"
"$SCRIPT_DIR"/sdwrap.py -u -i "$TMP" -o "$TMP2"
chtag -tc 819 "$TMP2"
sed -e "s/\(^ IMPORT \(DATA\|CODE\)64,\)'[^']*'/\1'$newdllname'/g" "$TMP2" > "$TMP"
"$SCRIPT_DIR"/sdwrap.py -i "$TMP" -o "$TMP2"

# Reformat sidedeck to be USS compatible
iconv -f ISO8859-1 -t IBM-1047 "$TMP2" > "$TMP"
dd conv=block cbs=80 if="$TMP" of="$outputsidedeck"
