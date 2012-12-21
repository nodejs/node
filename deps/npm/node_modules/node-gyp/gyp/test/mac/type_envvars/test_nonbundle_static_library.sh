#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

test $MACH_O_TYPE = staticlib
test $PRODUCT_TYPE = com.apple.product-type.library.static
test $PRODUCT_NAME = nonbundle_static_library
test $FULL_PRODUCT_NAME = libnonbundle_static_library.a

test $EXECUTABLE_NAME = libnonbundle_static_library.a
test $EXECUTABLE_PATH = libnonbundle_static_library.a
[[ ! $WRAPPER_NAME && ${WRAPPER_NAME-_} ]]

[[ ! $DYLIB_INSTALL_NAME_BASE && ${DYLIB_INSTALL_NAME_BASE-_} ]]
[[ ! $LD_DYLIB_INSTALL_NAME && ${LD_DYLIB_INSTALL_NAME-_} ]]

# Should be set, but empty.
[[ ! $SDKROOT && ! ${SDKROOT-_} ]]
