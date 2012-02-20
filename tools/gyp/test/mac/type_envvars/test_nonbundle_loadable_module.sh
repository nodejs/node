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
