/* Copyright (c) 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <Availability.h>

/* GYPTEST_MAC_VERSION_MIN: should be set to the corresponding value of
 * xcode setting 'MACOSX_DEPLOYMENT_TARGET', otherwise both should be
 * left undefined.
 *
 * GYPTEST_IOS_VERSION_MIN: should be set to the corresponding value of
 * xcode setting 'IPHONEOS_DEPLOYMENT_TARGET', otherwise both should be
 * left undefined.
 */

#if defined(GYPTEST_MAC_VERSION_MIN)
# if GYPTEST_MAC_VERSION_MIN != __MAC_OS_X_VERSION_MIN_REQUIRED
#  error __MAC_OS_X_VERSION_MIN_REQUIRED has wrong value
# endif
#elif defined(__MAC_OS_X_VERSION_MIN_REQUIRED)
# error __MAC_OS_X_VERSION_MIN_REQUIRED should be undefined
#endif

#if defined(GYPTEST_IOS_VERSION_MIN)
# if GYPTEST_IOS_VERSION_MIN != __IPHONE_OS_VERSION_MIN_REQUIRED
#  error __IPHONE_OS_VERSION_MIN_REQUIRED has wrong value
# endif
#elif defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
# error __IPHONE_OS_VERSION_MIN_REQUIRED should be undefined
#endif

int main() { return 0; }

