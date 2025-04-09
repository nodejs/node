#!/bin/sh
#
# Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# Build the Cloudflare quiche
#
set -e

SRCTOP="$(cd $SRCTOP; pwd)"
BLDTOP="$(cd $BLDTOP; pwd)"

echo "------------------------------------------------------------------"
echo "Building Cloudflare quiche"
echo "------------------------------------------------------------------"

QUICHE_TARGET_PATH="$BLDTOP/quiche"
test -d "$QUICHE_TARGET_PATH" || mkdir "$QUICHE_TARGET_PATH"
echo "   QUICHE_TARGET_PATH: $QUICHE_TARGET_PATH"

(cd "$SRCTOP/cloudflare-quiche" && cargo build --verbose --target-dir "$QUICHE_TARGET_PATH")
