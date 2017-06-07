#!/bin/bash

set -e
set -x

FULLVERSION="$FULLVERSION"
NPMVERSION="$NPMVERSION"

for dir in tools/osx-pkg/languages/*; do
  if [[ -d "$dir" ]]; then
    d=$(basename "$dir")
    mkdir -p tools/osx-pkg/resources/"$d"
    cat "$dir/welcome.rtf.tmpl" \
      | sed -E "s/\\{nodeversion\\}/$FULLVERSION/g" \
      | sed -E "s/\\{npmversion\\}/$NPMVERSION/g" \
      > tools/osx-pkg/resources/"$d"/welcome.rtf
  fi
done
