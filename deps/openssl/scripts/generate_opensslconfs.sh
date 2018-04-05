#!/bin/bash
# This script generates opensslconf.h headers for 
SCRIPTSDIR="`dirname \"$0\"`"

pushd ${SCRIPTSDIR}/../config
make clean
find archs -name opensslconf.h -exec rm "{}" \;
make

popd

pushd ${SCRIPTSDIR}/../openssl
perl ./Configure no-shared no-symlinks aix-gcc > /dev/null
if git diff-index --quiet HEAD . ; then
    git add .
    git commit . -F- <<EOF
deps: update openssl config files

Regenerate config files for supported platforms with Makefile.
EOF
else 
  echo "No changes openssl config files. Nothing to commit"
fi

popd
