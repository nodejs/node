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

test "${CONTENTS_FOLDER_PATH}" = "My App.app/Contents"
test "${EXECUTABLE_FOLDER_PATH}" = "My App.app/Contents/MacOS"
test "${UNLOCALIZED_RESOURCES_FOLDER_PATH}" = "My App.app/Contents/Resources"
test "${JAVA_FOLDER_PATH}" = "My App.app/Contents/Resources/Java"
test "${FRAMEWORKS_FOLDER_PATH}" = "My App.app/Contents/Frameworks"
test "${SHARED_FRAMEWORKS_FOLDER_PATH}" = "My App.app/Contents/SharedFrameworks"
test "${SHARED_SUPPORT_FOLDER_PATH}" = "My App.app/Contents/SharedSupport"
test "${PLUGINS_FOLDER_PATH}" = "My App.app/Contents/PlugIns"
test "${XPCSERVICES_FOLDER_PATH}" = "My App.app/Contents/XPCServices"

[[ ! $DYLIB_INSTALL_NAME_BASE && ${DYLIB_INSTALL_NAME_BASE-_} ]]
[[ ! $LD_DYLIB_INSTALL_NAME && ${LD_DYLIB_INSTALL_NAME-_} ]]

"$(dirname "$0")/test_check_sdkroot.sh"
