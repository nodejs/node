#!/bin/bash
cat ChangeLog | {
  s=-1
  while read line; do
    if [ "${line:0:1}" == "2" ]; then
      let "++s"
    fi
    if [ $s -eq 1 ]; then
      exit
    else
      echo "$line"
    fi
  done
}

