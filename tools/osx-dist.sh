#!/bin/sh

TOOLS=`dirname $0`
ROOT=$TOOLS/..

VERSION=`git describe`
CONTENTS=dist-osx/nodejs-$VERSION

# go build it in the root of the git repository
pushd $ROOT

./configure --prefix=/usr/local/nodejs
make
make install DESTDIR="$CONTENTS"

mkdir -p "$CONTENTS/usr/local/bin"
pushd "$CONTENTS/usr/local/bin"
ln -s ../nodejs/bin/* .
popd # $CONTENTS/usr/local/bin

popd # $ROOT

"$TOOLS/osx-pkg-dmg-create.sh" "$ROOT/$CONTENTS" NodeJS $VERSION 'org.nodejs'
