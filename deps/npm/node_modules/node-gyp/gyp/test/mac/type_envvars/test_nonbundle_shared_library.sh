#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

test $MACH_O_TYPE = mh_dylib
test $PRODUCT_TYPE = com.apple.product-type.library.dynamic
test $PRODUCT_NAME = nonbundle_shared_library
test $FULL_PRODUCT_NAME = libnonbundle_shared_library.dylib

test $EXECUTABLE_NAME = libnonbundle_shared_library.dylib
test $EXECUTABLE_PATH = libnonbundle_shared_library.dylib
[[ ! $WRAPPER_NAME && ${WRAPPER_NAME-_} ]]

test $DYLIB_INSTALL_NAME_BASE = "/usr/local/lib"
test $LD_DYLIB_INSTALL_NAME = "/usr/local/lib/libnonbundle_shared_library.dylib"

# Should be set, but empty.
[[ ! $SDKROOT && ! ${SDKROOT-_} ]]
