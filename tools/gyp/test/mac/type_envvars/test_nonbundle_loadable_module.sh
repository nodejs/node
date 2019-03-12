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

[[ ! $CONTENTS_FOLDER_PATH && ${CONTENTS_FOLDER_PATH-_} ]]
[[ ! $EXECUTABLE_FOLDER_PATH && ${EXECUTABLE_FOLDER_PATH-_} ]]
[[ ! $UNLOCALIZED_RESOURCES_FOLDER_PATH \
       && ${UNLOCALIZED_RESOURCES_FOLDER_PATH-_} ]]
[[ ! $JAVA_FOLDER_PATH && ${JAVA_FOLDER_PATH-_} ]]
[[ ! $FRAMEWORKS_FOLDER_PATH && ${FRAMEWORKS_FOLDER_PATH-_} ]]
[[ ! $SHARED_FRAMEWORKS_FOLDER_PATH && ${SHARED_FRAMEWORKS_FOLDER_PATH-_} ]]
[[ ! $SHARED_SUPPORT_FOLDER_PATH && ${SHARED_SUPPORT_FOLDER_PATH-_} ]]
[[ ! $PLUGINS_FOLDER_PATH && ${PLUGINS_FOLDER_PATH-_} ]]
[[ ! $XPCSERVICES_FOLDER_PATH && ${XPCSERVICES_FOLDER_PATH-_} ]]

test $DYLIB_INSTALL_NAME_BASE = "/usr/local/lib"
test $LD_DYLIB_INSTALL_NAME = "/usr/local/lib/nonbundle_loadable_module.so"

"$(dirname "$0")/test_check_sdkroot.sh"
