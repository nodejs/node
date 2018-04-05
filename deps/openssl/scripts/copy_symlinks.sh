#!/bin/bash
# This script removes symbolic links in ../openssl/include/openssl
# and replaces them with real header files which would otherwise 
# cause issues on Windows platforms.
SCRIPTSDIR="`dirname \"$0\"`"

pushd ${SCRIPTSDIR}/../openssl

## run config
./config

pushd include/openssl

## replace symlinks with real headers
for header in *.h
do
    if [ -L $header ]; then
        orig_header=`readlink $header`
        rm $header && cp $orig_header $header
    fi
done

## Replace opensslconfig.h in include/openssl dir (current)
rm opensslconf.h
echo '#include "../../crypto/opensslconf.h"' >  opensslconf.h

## Replace opensslconfig.h in crypto dir
rm ../../crypto/opensslconf.h
echo '#include "../../config/opensslconf.h"' > ../../crypto/opensslconf.h

popd

if git diff-index --quiet HEAD include crypto/opensslconf.h ; then
    git add include crypto/opensslconf.h 
    git commit include crypto/opensslconf.h -F- <<EOF
deps: copy all openssl header files to include dir

All symlink files in deps/openssl/openssl/include/openssl/ are removed
and replaced with real header files to avoid issues on Windows.
Two opensslconf.h files in the crypto and include dir are replaced to
refer config/opensslconf.h.
EOF
else
  echo "No changes in include or crypto/opensslconf.h. Nothing to commit"
fi

git clean -f
git checkout Makefile Makefile.bak

popd
