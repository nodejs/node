#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

test $MACH_O_TYPE = mh_bundle
test $PRODUCT_TYPE = com.apple.product-type.bundle
test $PRODUCT_NAME = bundle_loadable_module
test $FULL_PRODUCT_NAME = bundle_loadable_module.bundle

test $EXECUTABLE_NAME = bundle_loadable_module
test $EXECUTABLE_PATH = \
    "bundle_loadable_module.bundle/Contents/MacOS/bundle_loadable_module"
test $WRAPPER_NAME = bundle_loadable_module.bundle

[[ ! $DYLIB_INSTALL_NAME_BASE && ${DYLIB_INSTALL_NAME_BASE-_} ]]
[[ ! $LD_DYLIB_INSTALL_NAME && ${LD_DYLIB_INSTALL_NAME-_} ]]

# Should be set, but empty.
[[ ! $SDKROOT && ! ${SDKROOT-_} ]]
