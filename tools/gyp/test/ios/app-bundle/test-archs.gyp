# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
  ],
  'target_defaults': {
    'product_extension': 'bundle',
    'mac_bundle_resources': [
      'TestApp/English.lproj/InfoPlist.strings',
      'TestApp/English.lproj/MainMenu.xib',
    ],
    'link_settings': {
      'libraries': [
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
      ],
    },
    'xcode_settings': {
      'OTHER_CFLAGS': [
        '-fobjc-abi-version=2',
      ],
      'CODE_SIGNING_REQUIRED': 'NO',
      'SDKROOT': 'iphonesimulator',  # -isysroot
      'TARGETED_DEVICE_FAMILY': '1,2',
      'INFOPLIST_FILE': 'TestApp/TestApp-Info.plist',
      'IPHONEOS_DEPLOYMENT_TARGET': '7.0',
      'CONFIGURATION_BUILD_DIR':'build/Default',
    },
  },
  'targets': [
    {
      'target_name': 'TestNoArchs',
      'product_name': 'TestNoArchs',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'TestApp/main.m',
        'TestApp/only-compile-in-32-bits.m',
      ],
      'xcode_settings': {
        'VALID_ARCHS': [
          'i386',
          'x86_64',
          'arm64',
          'armv7',
        ],
      }
    },
    {
      'target_name': 'TestArch32Bits',
      'product_name': 'TestArch32Bits',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'TestApp/main.m',
        'TestApp/only-compile-in-32-bits.m',
      ],
      'xcode_settings': {
        'ARCHS': [
          '$(ARCHS_STANDARD)',
        ],
        'VALID_ARCHS': [
          'i386',
          'armv7',
        ],
      },
    },
    {
      'target_name': 'TestArch64Bits',
      'product_name': 'TestArch64Bits',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'TestApp/main.m',
        'TestApp/only-compile-in-64-bits.m',
      ],
      'xcode_settings': {
        'ARCHS': [
          '$(ARCHS_STANDARD_INCLUDING_64_BIT)',
        ],
        'VALID_ARCHS': [
          'x86_64',
          'arm64',
        ],
      },
    },
    {
      'target_name': 'TestMultiArchs',
      'product_name': 'TestMultiArchs',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'TestApp/main.m',
      ],
      'xcode_settings': {
        'ARCHS': [
          '$(ARCHS_STANDARD_INCLUDING_64_BIT)',
        ],
        'VALID_ARCHS': [
          'x86_64',
          'i386',
          'arm64',
          'armv7',
        ],
      }
    },
  ],
}
