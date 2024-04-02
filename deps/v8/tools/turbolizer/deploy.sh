#!/bin/bash

DEST=$1

if [ ! -d "$DEST" ]; then
  echo -e "Destination \"$DEST\" is not a directory. Run\n\tnpm deploy -- [destination-directory]"
  exit 1
fi

function copy() {
  echo -n "."
  cp "$@"
}

echo -n "Deploying..."
copy *.png $DEST/
copy *.css $DEST/
copy index.html $DEST/
copy info-view.html $DEST/
copy -R build $DEST/
copy -R img $DEST/
echo "done!"

echo "Deployed to $DEST/."
