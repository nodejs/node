#!/bin/sh
#
# Copyright 2017-2023 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2017, Oracle and/or its affiliates.  All rights reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# OpenSSL external testing using the Python Cryptography module
#
set -e
set -x

O_EXE=`pwd`/$BLDTOP/apps
O_BINC=`pwd`/$BLDTOP/include
O_SINC=`pwd`/$SRCTOP/include
O_LIB=`pwd`/$BLDTOP

export PATH=$O_EXE:$PATH
export LD_LIBRARY_PATH=$O_LIB:$LD_LIBRARY_PATH

# Check/Set openssl version
OPENSSL_VERSION=`openssl version | cut -f 2 -d ' '`

echo "------------------------------------------------------------------"
echo "Testing OpenSSL using Python Cryptography:"
echo "   CWD:                $PWD"
echo "   SRCTOP:             $SRCTOP"
echo "   BLDTOP:             $BLDTOP"
echo "   OpenSSL version:    $OPENSSL_VERSION"
echo "------------------------------------------------------------------"

cd $SRCTOP

# Create a python virtual env and activate
rm -rf venv-cryptography
python -m venv venv-cryptography
. ./venv-cryptography/bin/activate

cd pyca-cryptography

echo "------------------------------------------------------------------"
echo "Building cryptography"
echo "------------------------------------------------------------------"
LDFLAGS="-L$O_LIB" CFLAGS="-I$O_BINC -I$O_SINC" pip install .[test]
pip install -e vectors

echo "------------------------------------------------------------------"
echo "Running tests"
echo "------------------------------------------------------------------"

pytest -n auto tests --wycheproof-root=../wycheproof

cd ../
deactivate
rm -rf venv-cryptography

exit 0

