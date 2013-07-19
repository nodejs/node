#!/bin/sh

SED=sed
UNAME=`uname`

if [ "$UNAME" = Darwin ] || [ "$UNAME" = FreeBSD ]; then
  SED=gsed
fi

cd `dirname "$0"`/../

for FILE in src/*.cc; do
  $SED -rne 's/^using (\w+::\w+);$/\1/p' $FILE | sort -c || echo "in $FILE"
done

for FILE in src/*.cc; do
  for IMPORT in `$SED -rne 's/^using (\w+)::(\w+);$/\2/p' $FILE`; do
    if ! $SED -re '/^using (\w+)::(\w+);$/d' $FILE | grep -q "$IMPORT"; then
      echo "$IMPORT unused in $FILE"
    fi
  done
done
