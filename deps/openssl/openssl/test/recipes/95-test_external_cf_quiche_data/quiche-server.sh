#!/bin/sh
#
# Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# Run the quiche server
#
set -e

SRCTOP="$(cd $SRCTOP; pwd)"
BLDTOP="$(cd $BLDTOP; pwd)"

echo "------------------------------------------------------------------"
echo "Running Cloudflare quiche-server"
echo "------------------------------------------------------------------"

QUICHE_TARGET_PATH="$BLDTOP/quiche"
test -d "$QUICHE_TARGET_PATH" || exit 1

"$QUICHE_TARGET_PATH/debug/quiche-server" --cert "$SRCTOP/test/certs/servercert.pem" \
    --key "$SRCTOP/test/certs/serverkey.pem" --disable-gso \
    --http-version HTTP/0.9 --root "$SRCTOP" --no-grease --disable-hystart &

echo $! >server.pid
