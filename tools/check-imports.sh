#!/bin/sh

cd `dirname "$0"`/../

for FILE in src/*.cc; do
  sed -rne 's/^using (\w+::\w+);$/\1/p' $FILE | sort -c || echo "in $FILE"
done

for FILE in src/*.cc; do
  for IMPORT in `sed -rne 's/^using (\w+)::(\w+);$/\2/p' $FILE`; do
    if ! sed -re '/^using (\w+)::(\w+);$/d' $FILE | grep -q "$IMPORT"; then
      echo "$IMPORT unused in $FILE"
    fi
  done
done
