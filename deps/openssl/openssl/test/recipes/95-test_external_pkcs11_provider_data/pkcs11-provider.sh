#!/bin/sh
#
# Copyright 2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# OpenSSL external testing using the pkcs11-provider
#

PWD="$(pwd)"

SRCTOP="$(cd $SRCTOP; pwd)"
BLDTOP="$(cd $BLDTOP; pwd)"

if [ "$SRCTOP" != "$BLDTOP" ] ; then
    echo "Out of tree builds not supported with pkcsa11-provider test!"
    exit 1
fi

O_EXE="$BLDTOP/apps"
O_BINC="$BLDTOP/include"
O_SINC="$SRCTOP/include"
O_LIB="$BLDTOP"

unset OPENSSL_CONF

export PATH="$O_EXE:$PATH"
export LD_LIBRARY_PATH="$O_LIB:$LD_LIBRARY_PATH"
export OPENSSL_ROOT_DIR="$O_LIB"

# Check/Set openssl version
OPENSSL_VERSION=`openssl version | cut -f 2 -d ' '`

echo "------------------------------------------------------------------"
echo "Testing OpenSSL using pkcs11-provider:"
echo "   CWD:                $PWD"
echo "   SRCTOP:             $SRCTOP"
echo "   BLDTOP:             $BLDTOP"
echo "   OPENSSL_ROOT_DIR:   $OPENSSL_ROOT_DIR"
echo "   OpenSSL version:    $OPENSSL_VERSION"
echo "------------------------------------------------------------------"

PKCS11_PROVIDER_BUILDDIR=$OPENSSL_ROOT_DIR/pkcs11-provider/builddir

echo "------------------------------------------------------------------"
echo "Building pkcs11-provider"
echo "------------------------------------------------------------------"

PKG_CONFIG_PATH="$BLDTOP" meson setup $PKCS11_PROVIDER_BUILDDIR $OPENSSL_ROOT_DIR/pkcs11-provider/ || exit 1
meson compile -C $PKCS11_PROVIDER_BUILDDIR pkcs11 || exit 1

echo "------------------------------------------------------------------"
echo "Running tests"
echo "------------------------------------------------------------------"

# The OpenSSL app uses ${HARNESS_OSSL_PREFIX} as a prefix for its standard output
HARNESS_OSSL_PREFIX= meson test -C $PKCS11_PROVIDER_BUILDDIR

if [ $? -ne 0 ]; then
    cat $PKCS11_PROVIDER_BUILDDIR/meson-logs/testlog.txt
    exit 1
fi

rm -rf $PKCS11_PROVIDER_BUILDDIR

exit 0
