#!/bin/bash

SELF_PATH="$0"
if [ "${SELF_PATH:0:1}" != "." ] && [ "${SELF_PATH:0:1}" != "/" ]; then
  SELF_PATH=./"$SELF_PATH"
fi
SELF_PATH=$( cd -P -- "$(dirname -- "$SELF_PATH")" \
          && pwd -P \
          ) && SELF_PATH=$SELF_PATH/$(basename -- "$0")

# resolve symlinks
while [ -h "$SELF_PATH" ]; do
  DIR=$(dirname -- "$SELF_PATH")
  SYM=$(readlink -- "$SELF_PATH")
  SELF_PATH=$( cd -- "$DIR" \
            && cd -- $(dirname -- "$SYM") \
            && pwd \
            )/$(basename -- "$SYM")
done
DIR=$( dirname -- "$SELF_PATH" )

export npm_config_root=$DIR/root
export npm_config_binroot=$DIR/bin

rm -rf $DIR/{root,bin}
mkdir -p $DIR/root
mkdir -p $DIR/bin
npm ls installed 2>/dev/null | grep -v npm | awk '{print $1}' | xargs npm rm &>/dev/null
npm install \
	base64@1.0.0 \
	eyes@0.1.1 \
	vows@0.2.5 \
	websocket-server@1.0.5 &>/dev/null
npm install ./test/packages/blerg &>/dev/null
npm install vows@0.3.0 &>/dev/null

echo ""
echo "##"
echo "## starting update"
echo "##"
echo ""

npm update

echo ""
echo "##"
echo "## update done, all should be 'latest'"
echo "##"
echo ""

list=$( npm ls installed remote 2>/dev/null )
echo "$list"
notlatest=$( echo "$list" | grep -v latest )
if [ "$notlatest" != "" ]; then
	echo "Failed: not latest"
	echo $notlatest
else
	echo "ok"
fi
