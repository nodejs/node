#!/bin/bash

# Collect the number of [[deprecated]] calls detected when compiling V8.
# Requires "v8_deprecate_get_isolate = true" to be useful.

set -e

if [ -z "$1" ]; then
  (>&2 echo "Usage: collect_deprecation_stats.sh [<outdir>|<log>]")
  exit 1
fi

if [ -d "$1" ]; then
  OUTDIR=$1
  FULL_LOG=/tmp/get_isolate_deprecation.log
  gn clean "$OUTDIR"
  autoninja -C "$OUTDIR" > $FULL_LOG
else
  FULL_LOG=$1
fi

FILTERED_LOG=/tmp/filtered_isolate_deprecation.log
UNIQUE_WARNINGS_LOG=/tmp/unique_warnings.log

grep "warning:" "$FULL_LOG" | sed $'
s|^\.\./\.\./||;
s/: warning: \'/: /;

# strip everything after deprecated function name (including template param).
s/\(<.*>\)\\?\'.*//' > $FILTERED_LOG

sort -u $FILTERED_LOG > $UNIQUE_WARNINGS_LOG

echo "Total deprecated calls: $(wc -l < $UNIQUE_WARNINGS_LOG)"
cut -f2 -d' ' $UNIQUE_WARNINGS_LOG | sort | uniq -c
