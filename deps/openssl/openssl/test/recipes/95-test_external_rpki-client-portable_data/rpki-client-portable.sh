#!/bin/sh -ex
#
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# krb5's test suite clears LD_LIBRARY_PATH
LDFLAGS="-L`pwd`/$BLDTOP -Wl,-rpath,`pwd`/$BLDTOP"
CFLAGS="-I`pwd`/$BLDTOP/include -I`pwd`/$SRCTOP/include"

WGET_OPTS='--no-check-certificate'
wget $WGET_OPTS -O $RPKI_TARBALL $RPKI_DOWNLOAD_URL && tar xzf $RPKI_TARBALL
wget $WGET_OPTS -O $RPKI_TARBALL.asc $RPKI_DOWNLOAD_URL.asc
#
# public key comes from
# https://cdn.openbsd.org/pub/OpenBSD/rpki-client/RELEASE_KEY.asc
#
cat <<EOF | gpg --import -
-----BEGIN PGP PUBLIC KEY BLOCK-----

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
zyzSqbo//+SSFHZG
=O6H8
-----END PGP PUBLIC KEY BLOCK-----
EOF
gpg --verify $RPKI_TARBALL.asc $RPKI_TARBALL || exit 1

cd $RPKI_SRC

./configure --with-openssl-cflags="$CFLAGS" --with-openssl-ldflags="$LDFLAGS" \
            CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS"

# quiet make so that Travis doesn't overflow
make

make check
