# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['"<(GENERATOR)"=="ninja"', {
      'make_global_settings': [
        ['CC', '/usr/bin/clang'],
        ['CXX', '/usr/bin/clang++'],
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'test_app',
      'product_name': 'Test App Assets Catalog Gyp',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'TestApp/main.m',
      ],
      'mac_bundle_resources': [
        'TestApp/English.lproj/InfoPlist.strings',
        'TestApp/English.lproj/MainMenu.xib',
        'TestApp/English.lproj/Main_iPhone.storyboard',
        'TestApp/Images.xcassets',
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
        'INFOPLIST_FILE': 'TestApp/TestApp-Info.plist',
        'SDKROOT': 'iphonesimulator',  # -isysroot
        'IPHONEOS_DEPLOYMENT_TARGET': '7.0',
        'CONFIGURATION_BUILD_DIR':'build/Default',
      },
    },
  ],
}
