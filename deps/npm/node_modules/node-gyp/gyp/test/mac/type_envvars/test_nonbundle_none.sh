#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# Check for "not set", not just "empty":
[[ ! $MACH_O_TYPE && ${MACH_O_TYPE-_} ]]
[[ ! $PRODUCT_TYPE && ${PRODUCT_TYPE-_} ]]
test $PRODUCT_NAME = nonbundle_none
[[ ! $FULL_PRODUCT_NAME && ${FULL_PRODUCT_NAME-_} ]]

[[ ! $EXECUTABLE_NAME && ${EXECUTABLE_NAME-_} ]]
[[ ! $EXECUTABLE_PATH && ${EXECUTABLE_PATH-_} ]]
[[ ! $WRAPPER_NAME && ${WRAPPER_NAME-_} ]]

[[ ! $DYLIB_INSTALL_NAME_BASE && ${DYLIB_INSTALL_NAME_BASE-_} ]]
[[ ! $LD_DYLIB_INSTALL_NAME && ${LD_DYLIB_INSTALL_NAME-_} ]]

# Should be set, but empty.
[[ ! $SDKROOT && ! ${SDKROOT-_} ]]
