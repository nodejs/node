#!/bin/sh
#
# Copyright 2022-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# OpenSSL external testing using the OQS provider
#
set -e

PWD="$(pwd)"

SRCTOP="$(cd $SRCTOP; pwd)"
BLDTOP="$(cd $BLDTOP; pwd)"

if [ "$SRCTOP" != "$BLDTOP" ] ; then
    echo "Out of tree builds not supported with oqsprovider test!"
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
echo "Testing OpenSSL using oqsprovider:"
echo "   CWD:                $PWD"
echo "   SRCTOP:             $SRCTOP"
echo "   BLDTOP:             $BLDTOP"
echo "   OPENSSL_ROOT_DIR:   $OPENSSL_ROOT_DIR"
echo "   OpenSSL version:    $OPENSSL_VERSION"
echo "------------------------------------------------------------------"

if [ ! -d $SRCTOP/oqs-provider/.local ]; then
# this version of oqsprovider dependent on v0.11.0 of liboqs, so set this;
# also be sure to use this openssl for liboqs-internal OpenSSL use;
# see all libops config options listed at
# https://github.com/open-quantum-safe/liboqs/wiki/Customizing-liboqs
(
       cd $SRCTOP/oqs-provider \
           && git clone --depth 1 --branch 0.11.0 https://github.com/open-quantum-safe/liboqs.git \
           && cd liboqs \
           && mkdir build \
           && cd build \
           && cmake -DOPENSSL_ROOT_DIR=$OPENSSL_ROOT_DIR -DCMAKE_INSTALL_PREFIX=$SRCTOP/oqs-provider/.local .. \
           && make \
           && make install
   )
fi

echo "   CWD:                $PWD"
liboqs_DIR=$SRCTOP/oqs-provider/.local cmake $SRCTOP/oqs-provider -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" -B _build && cmake --build _build
export CTEST_OUTPUT_ON_FAILURE=1
export HARNESS_OSSL_PREFIX=''
export OPENSSL_APP="$O_EXE/openssl"
export OPENSSL_MODULES=$PWD/_build/lib
export OQS_PROVIDER_TESTSCRIPTS=$SRCTOP/oqs-provider/scripts
export OPENSSL_CONF=$OQS_PROVIDER_TESTSCRIPTS/openssl-ca.cnf
# Be verbose if harness is verbose:
$SRCTOP/oqs-provider/scripts/runtests.sh -V
