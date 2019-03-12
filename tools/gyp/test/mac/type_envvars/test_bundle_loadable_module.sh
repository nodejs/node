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

test $CONTENTS_FOLDER_PATH = bundle_loadable_module.bundle/Contents
test $EXECUTABLE_FOLDER_PATH = bundle_loadable_module.bundle/Contents/MacOS
test $UNLOCALIZED_RESOURCES_FOLDER_PATH = \
    bundle_loadable_module.bundle/Contents/Resources
test $JAVA_FOLDER_PATH = bundle_loadable_module.bundle/Contents/Resources/Java
test $FRAMEWORKS_FOLDER_PATH = bundle_loadable_module.bundle/Contents/Frameworks
test $SHARED_FRAMEWORKS_FOLDER_PATH = \
    bundle_loadable_module.bundle/Contents/SharedFrameworks
test $SHARED_SUPPORT_FOLDER_PATH = \
    bundle_loadable_module.bundle/Contents/SharedSupport
test $PLUGINS_FOLDER_PATH = bundle_loadable_module.bundle/Contents/PlugIns
test $XPCSERVICES_FOLDER_PATH = \
    bundle_loadable_module.bundle/Contents/XPCServices

[[ ! $DYLIB_INSTALL_NAME_BASE && ${DYLIB_INSTALL_NAME_BASE-_} ]]
[[ ! $LD_DYLIB_INSTALL_NAME && ${LD_DYLIB_INSTALL_NAME-_} ]]

"$(dirname "$0")/test_check_sdkroot.sh"
