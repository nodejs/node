#!/bin/sh

TOOLS=`dirname $0`
ROOT=$TOOLS/..

VERSION=`python $TOOLS/msvs/msi/getnodeversion.py < $ROOT/src/node_version.h`
CONTENTS=$ROOT/dist-osx
PMDOC=$TOOLS/osx-pkg.pmdoc
VENDOR='org.nodejs'
NAME=NodeJS

# go build it in the root of the git repository
pushd $ROOT

./configure --prefix=/usr/local
make
make install DESTDIR="$CONTENTS"

popd # $ROOT

PKGID="$VENDOR.$NAME-$VERSION"

packagemaker=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker

$packagemaker \
  --id "$PKGID" \
  --doc $PMDOC \
  --out $CONTENTS/node-$VERSION.pkg
