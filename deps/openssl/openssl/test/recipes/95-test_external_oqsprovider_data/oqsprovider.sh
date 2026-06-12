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
INSTALLTOP=$BLDTOP/oqs-provider/.local

# Prepare minimal local OpenSSL "installation"
mkdir -p $INSTALLTOP/bin
mkdir -p $INSTALLTOP/include/openssl
mkdir -p $INSTALLTOP/lib
ln -sf $SRCTOP/include/openssl/*.h $INSTALLTOP/include/openssl/
ln -sf $BLDTOP/include/openssl/*.h $INSTALLTOP/include/openssl/
ln -sf $BLDTOP/libcrypto.so.$SHLIB_VERSION_NUMBER $INSTALLTOP/lib/
ln -sf $BLDTOP/libssl.so.$SHLIB_VERSION_NUMBER $INSTALLTOP/lib/
ln -sf libcrypto.so.$SHLIB_VERSION_NUMBER $INSTALLTOP/lib/libcrypto.so
ln -sf libssl.so.$SHLIB_VERSION_NUMBER $INSTALLTOP/lib/libssl.so
ln -sf $BLDTOP/apps/openssl $INSTALLTOP/bin/

O_EXE="$INSTALLTOP/bin"
O_INC="$INSTALLTOP/include"
O_LIB="$INSTALLTOP/lib"

unset OPENSSL_CONF

export PATH="$O_EXE:$PATH"
export LD_LIBRARY_PATH="$O_LIB:$LD_LIBRARY_PATH"

# Check/Set openssl version
OPENSSL_VERSION=`openssl version | cut -f 2 -d ' '`

echo "------------------------------------------------------------------"
echo "Testing OpenSSL using oqsprovider:"
echo "   CWD:                $PWD"
echo "   SRCTOP:             $SRCTOP"
echo "   BLDTOP:             $BLDTOP"
echo "   INSTALLTOP:         $INSTALLTOP"
echo "   OpenSSL version:    $OPENSSL_VERSION"
echo "------------------------------------------------------------------"

if [ ! -f $INSTALLTOP/lib/liboqs.a ]; then
# this version of oqsprovider dependent on v0.11.0 of liboqs, so set this;
# also be sure to use this openssl for liboqs-internal OpenSSL use;
# see all libops config options listed at
# https://github.com/open-quantum-safe/liboqs/wiki/Customizing-liboqs
(
        cd $SRCTOP/oqs-provider \
           && rm -rf liboqs \
           && git clone --depth 1 --branch 0.11.0 https://github.com/open-quantum-safe/liboqs.git \
           && cd liboqs \
           && mkdir build \
           && cd build \
           && cmake -DOPENSSL_ROOT_DIR=$INSTALLTOP -DCMAKE_INSTALL_PREFIX=$INSTALLTOP .. \
           && make \
           && make install
   )
fi

echo "   CWD:                $PWD"
liboqs_DIR=$INSTALLTOP cmake $SRCTOP/oqs-provider -DOPENSSL_ROOT_DIR="$INSTALLTOP" -B _build && cmake --build _build
export CTEST_OUTPUT_ON_FAILURE=1
export OPENSSL_APP="$O_EXE/openssl"
export OPENSSL_MODULES=$PWD/_build/lib
export OQS_PROVIDER_TESTSCRIPTS=$SRCTOP/oqs-provider/scripts
export OPENSSL_CONF=$OQS_PROVIDER_TESTSCRIPTS/openssl-ca.cnf
# hotfix for wrong cert validity period
cp $SRCTOP/test/recipes/95-test_external_oqsprovider_data/oqsprovider-ca.sh $SRCTOP/oqs-provider/scripts/
# Be verbose if harness is verbose:
$SRCTOP/oqs-provider/scripts/runtests.sh -V
