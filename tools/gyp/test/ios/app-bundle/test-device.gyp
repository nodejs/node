# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['"<(GENERATOR)"=="xcode"', {
      'target_defaults': {
        'configurations': {
          'Default': {
            'xcode_settings': {
              'SDKROOT': 'iphonesimulator',
              'CONFIGURATION_BUILD_DIR':'build/Default',
            }
          },
          'Default-iphoneos': {
            'xcode_settings': {
              'SDKROOT': 'iphoneos',
              'CONFIGURATION_BUILD_DIR':'build/Default-iphoneos',
            }
          },
        },
      },
    }, {
      'target_defaults': {
        'configurations': {
          'Default': {
            'xcode_settings': {
              'SDKROOT': 'iphonesimulator',
            }
          },
        },
      },
    }],
  ],
  'targets': [
    {
      'target_name': 'test_app',
      'product_name': 'Test App Gyp',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'TestApp/main.m',
      ],
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
        'SDKROOT': 'iphonesimulator',  # -isysroot
        'TARGETED_DEVICE_FAMILY': '1,2',
        'INFOPLIST_OUTPUT_FORMAT':'xml',
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'INFOPLIST_FILE': 'TestApp/TestApp-Info.plist',
        'IPHONEOS_DEPLOYMENT_TARGET': '8.0',
        'CODE_SIGNING_REQUIRED': 'NO',
        'CODE_SIGN_IDENTITY[sdk=iphoneos*]': '',

      },
    },
    {
      'target_name': 'sig_test',
      'product_name': 'sigtest',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'TestApp/main.m',
      ],
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
      'postbuilds': [
        {
          'postbuild_name': 'Verify no signature',
          'action': [
            'python',
            'TestApp/check_no_signature.py'
          ],
        },
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-fobjc-abi-version=2',
        ],
        'SDKROOT': 'iphonesimulator',  # -isysroot
        'CODE_SIGN_IDENTITY[sdk=iphoneos*]': 'iPhone Developer',
        'INFOPLIST_OUTPUT_FORMAT':'xml',
        'INFOPLIST_FILE': 'TestApp/TestApp-Info.plist',
        'IPHONEOS_DEPLOYMENT_TARGET': '8.0',
        'CONFIGURATION_BUILD_DIR':'buildsig/Default',
      },
    },
  ],
}
