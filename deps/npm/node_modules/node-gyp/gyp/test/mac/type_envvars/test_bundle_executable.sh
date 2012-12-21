#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

test $MACH_O_TYPE = mh_execute
test $PRODUCT_TYPE = com.apple.product-type.application
test "${PRODUCT_NAME}" = "My App"
test "${FULL_PRODUCT_NAME}" = "My App.app"

test "${EXECUTABLE_NAME}" = "My App"
test "${EXECUTABLE_PATH}" = "My App.app/Contents/MacOS/My App"
test "${WRAPPER_NAME}" = "My App.app"

[[ ! $DYLIB_INSTALL_NAME_BASE && ${DYLIB_INSTALL_NAME_BASE-_} ]]
[[ ! $LD_DYLIB_INSTALL_NAME && ${LD_DYLIB_INSTALL_NAME-_} ]]

# Should be set, but empty.
[[ ! $SDKROOT && ! ${SDKROOT-_} ]]
