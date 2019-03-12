# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['"<(GENERATOR)"=="ninja"', {
      'make_global_settings': [
        ['CC', '/usr/bin/clang'],
        ['CXX', '/usr/bin/clang++'],
      ],
    }]
  ],
  'target_defaults': {
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-fobjc-abi-version=2',
        ],
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'SDKROOT': 'iphonesimulator',  # -isysroot
        'CONFIGURATION_BUILD_DIR':'build/Default',
        'IPHONEOS_DEPLOYMENT_TARGET': '9.0',
        'CODE_SIGN_IDENTITY[sdk=iphoneos*]': 'iPhone Developer',
      }
  },
  'targets': [
    {
      'target_name': 'app_under_test',
      'type': 'executable',
      'mac_bundle': 1,
      'mac_bundle_resources': [
        'App/Base.lproj/LaunchScreen.xib',
        'App/Base.lproj/Main.storyboard',
      ],
      'sources': [
        'App/AppDelegate.h',
        'App/AppDelegate.m',
        'App/ViewController.h',
        'App/ViewController.m',
        'App/main.m',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
        ],
      },
      'xcode_settings': {
        'INFOPLIST_FILE': 'App/Info.plist',
      },
    },
    {
      'target_name': 'app_tests',
      'type': 'loadable_module',
      'mac_xctest_bundle': 1,
      'sources': [
        'AppTests/AppTests.m',
      ],
      'dependencies': [
        'app_under_test'
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        ],
      },
      'xcode_settings': {
        'WRAPPER_EXTENSION': 'xctest',
        'INFOPLIST_FILE': 'AppTests/Info.plist',
        'BUNDLE_LOADER': '$(BUILT_PRODUCTS_DIR)/app_under_test.app/app_under_test',
        'TEST_HOST': '$(BUNDLE_LOADER)',
      },
    },
  ],
}
