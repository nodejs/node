#!/bin/sh

OBJ=$1
shift
PROG=$1
shift

if test -z $OBJ; then
  echo "This script is supposed to be run from 'make'."
  exit 1
fi

echo "====> Benchmark results for $PROG" >$PROG-bench
size -t $OBJ | tail -n 1 >>$PROG-bench
wc -c $PROG >>$PROG-bench

for i in $@; do
  echo "== $i" >>$PROG-bench
  cat $i >/dev/null
  sh -c "time ./$PROG $i" >>$PROG-bench 2>&1
  sh -c "time ./$PROG $i" >>$PROG-bench 2>&1
done
