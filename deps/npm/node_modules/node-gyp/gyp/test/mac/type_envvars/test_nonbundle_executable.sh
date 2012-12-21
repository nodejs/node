#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
# For some reason, Xcode doesn't set MACH_O_TYPE for non-bundle executables.
# Check for "not set", not just "empty":
[[ ! $MACH_O_TYPE && ${MACH_O_TYPE-_} ]]
test $PRODUCT_TYPE = com.apple.product-type.tool
test $PRODUCT_NAME = nonbundle_executable
test $FULL_PRODUCT_NAME = nonbundle_executable

test $EXECUTABLE_NAME = nonbundle_executable
test $EXECUTABLE_PATH = nonbundle_executable
[[ ! $WRAPPER_NAME && ${WRAPPER_NAME-_} ]]

[[ ! $DYLIB_INSTALL_NAME_BASE && ${DYLIB_INSTALL_NAME_BASE-_} ]]
[[ ! $LD_DYLIB_INSTALL_NAME && ${LD_DYLIB_INSTALL_NAME-_} ]]

# Should be set, but empty.
[[ ! $SDKROOT && ! ${SDKROOT-_} ]]
