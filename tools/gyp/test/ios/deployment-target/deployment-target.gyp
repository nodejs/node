# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
    ['CXX', '/usr/bin/clang++'],
  ],
  'targets': [
    {
      'target_name': 'version-min-4.3',
      'type': 'static_library',
      'sources': [ 'check-version-min.c', ],
      'defines': [ 'GYPTEST_IOS_VERSION_MIN=40300', ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'SDKROOT': 'iphoneos',
        'IPHONEOS_DEPLOYMENT_TARGET': '4.3',
      },
    },
    {
      'target_name': 'version-min-5.0',
      'type': 'static_library',
      'sources': [ 'check-version-min.c', ],
      'defines': [ 'GYPTEST_IOS_VERSION_MIN=50000', ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'SDKROOT': 'iphoneos',
        'IPHONEOS_DEPLOYMENT_TARGET': '5.0',
      },
    }
  ],
}

