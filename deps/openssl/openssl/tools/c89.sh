#!/bin/sh -k
#
# Re-order arguments so that -L comes first
#
opts=""
lopts=""
        
for arg in $* ; do
  case $arg in
    -L*) lopts="$lopts $arg" ;;
    *) opts="$opts $arg" ;;
  esac
done

c89 $lopts $opts
