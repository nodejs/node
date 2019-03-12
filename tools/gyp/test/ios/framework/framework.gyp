# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'iOSFramework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [
        'iOSFramework/iOSFramework.h',
        'iOSFramework/Thing.h',
        'iOSFramework/Thing.m',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
        ],
      },
      'mac_framework_headers': [
        # Using two headers here tests mac_tool.py NextGreaterPowerOf2.
        'iOSFramework/iOSFramework.h',
        'iOSFramework/Thing.h',
      ],
      'mac_framework_dirs': [
        '$(SDKROOT)/../../Library/Frameworks',
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-fobjc-abi-version=2',
        ],
        'INFOPLIST_FILE': 'iOSFramework/Info.plist',
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'SDKROOT': 'iphoneos',
        'IPHONEOS_DEPLOYMENT_TARGET': '8.0',
        'CONFIGURATION_BUILD_DIR':'build/Default',
        'CODE_SIGN_IDENTITY[sdk=iphoneos*]': 'iPhone Developer',
      },
    },
  ],
}
