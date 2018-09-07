#!/bin/bash

DEST=$1

if [ ! -d "$DEST" ]; then
  echo -e "Destination \"$DEST\" is not a directory. Run\n\tnpm deploy -- [destination-directory]"
  exit 1
fi

echo "Deploying..."

cp *.jpg $DEST/
cp *.png $DEST/
cp *.css $DEST/
cp index.html $DEST/
cp -R build $DEST/

echo "Deployed to $DEST/."
