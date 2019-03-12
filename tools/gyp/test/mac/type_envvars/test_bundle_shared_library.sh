#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

test $MACH_O_TYPE = mh_dylib
test $PRODUCT_TYPE = com.apple.product-type.framework
test $PRODUCT_NAME = bundle_shared_library
test $FULL_PRODUCT_NAME = bundle_shared_library.framework

test $EXECUTABLE_NAME = bundle_shared_library
test $EXECUTABLE_PATH = \
    "bundle_shared_library.framework/Versions/A/bundle_shared_library"
test $WRAPPER_NAME = bundle_shared_library.framework

test $CONTENTS_FOLDER_PATH = bundle_shared_library.framework/Versions/A
test $EXECUTABLE_FOLDER_PATH = bundle_shared_library.framework/Versions/A
test $UNLOCALIZED_RESOURCES_FOLDER_PATH = \
    bundle_shared_library.framework/Versions/A/Resources
test $JAVA_FOLDER_PATH = \
    bundle_shared_library.framework/Versions/A/Resources/Java
test $FRAMEWORKS_FOLDER_PATH = \
    bundle_shared_library.framework/Versions/A/Frameworks
test $SHARED_FRAMEWORKS_FOLDER_PATH = \
    bundle_shared_library.framework/Versions/A/SharedFrameworks
test $SHARED_SUPPORT_FOLDER_PATH = \
    bundle_shared_library.framework/Versions/A/Resources
test $PLUGINS_FOLDER_PATH = bundle_shared_library.framework/Versions/A/PlugIns
test $XPCSERVICES_FOLDER_PATH = \
    bundle_shared_library.framework/Versions/A/XPCServices

test $DYLIB_INSTALL_NAME_BASE = "/Library/Frameworks"
test $LD_DYLIB_INSTALL_NAME = \
    "/Library/Frameworks/bundle_shared_library.framework/Versions/A/bundle_shared_library"

"$(dirname "$0")/test_check_sdkroot.sh"
