#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

test $MACH_O_TYPE = mh_bundle
test $PRODUCT_TYPE = com.apple.product-type.library.dynamic
test $PRODUCT_NAME = nonbundle_loadable_module
test $FULL_PRODUCT_NAME = nonbundle_loadable_module.so

test $EXECUTABLE_NAME = nonbundle_loadable_module.so
test $EXECUTABLE_PATH = nonbundle_loadable_module.so
[[ ! $WRAPPER_NAME && ${WRAPPER_NAME-_} ]]

test $DYLIB_INSTALL_NAME_BASE = "/usr/local/lib"
test $LD_DYLIB_INSTALL_NAME = "/usr/local/lib/nonbundle_loadable_module.so"

# Should be set, but empty.
[[ ! $SDKROOT && ! ${SDKROOT-_} ]]
