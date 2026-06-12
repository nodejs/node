#!/bin/bash
#
# Copyright 2021-2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# OpenSSL external testing using the TLSFuzzer test suite
#
set -e
set -x

PWD="$(pwd)"

SRCTOP="$(cd $SRCTOP; pwd)"
BLDTOP="$(cd $BLDTOP; pwd)"

if [ "$SRCTOP" != "$BLDTOP" ] ; then
    echo "Out of tree builds not supported with TLSFuzzer test!"
    exit 1
fi

O_EXE="$BLDTOP/apps"
O_BINC="$BLDTOP/include"
O_SINC="$SRCTOP/include"
O_LIB="$BLDTOP"

export PATH="$O_EXE:$PATH"
export LD_LIBRARY_PATH="$O_LIB:$LD_LIBRARY_PATH"
export OPENSSL_ROOT_DIR="$O_LIB"


CLI="${O_EXE}/openssl"
SERV="${O_EXE}/openssl"

# Check/Set openssl version
OPENSSL_VERSION=$($CLI version | cut -f 2 -d ' ')

TMPFILE="${PWD}/tls-fuzzer.$$.tmp"
PSKFILE="${PWD}/tls-fuzzer.psk.$$.tmp"

PYTHON=`which python3`
PORT=4433

echo "------------------------------------------------------------------"
echo "Testing OpenSSL using TLSFuzzer:"
echo "   CWD:                $PWD"
echo "   SRCTOP:             $SRCTOP"
echo "   BLDTOP:             $BLDTOP"
echo "   OPENSSL_ROOT_DIR:   $OPENSSL_ROOT_DIR"
echo "   Python:             $PYTHON"
echo "   TESTDATADIR:        $TESTDATADIR"
echo "   OPENSSL_VERSION:    $OPENSSL_VERSION"
echo "------------------------------------------------------------------"

cd "${SRCTOP}/tlsfuzzer"

test -L ecdsa || ln -s ../python-ecdsa/src/ecdsa ecdsa
test -L tlslite || ln -s ../tlslite-ng/tlslite tlslite 2>/dev/null

retval=0

tls_fuzzer_prepare

PYTHONPATH=. "${PYTHON}" tests/scripts_retention.py ${TMPFILE} ${SERV} 821
retval=$?

rm -f ${TMPFILE}
[ -f "${PSKFILE}" ] && rm -f ${PSKFILE}

cd $PWD

exit $retval
