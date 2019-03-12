# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'xcode_settings': {
      'SDKROOT': 'iphoneos',
      'FRAMEWORK_SEARCH_PATHS': [
        '$(inherited)',
        '$(DEVELOPER_FRAMEWORKS_DIR)',
      ],
      'OTHER_LDFLAGS': [
        '$(inherited)',
        '-ObjC',
      ],
      'GCC_PREFIX_HEADER': '',
      'CLANG_ENABLE_OBJC_ARC': 'YES',
      'INFOPLIST_FILE': 'Info.plist',
    },
  },
  'targets': [
    {
      'target_name': 'testApp',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'MyAppDelegate.h',
        'MyAppDelegate.m',
        'main.m',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
        ],
      },
    },
    {
      'target_name': 'tests',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'mac_xcuitest_bundle': 1,
      'sources': [
        'TestCase.m',
      ],
      'dependencies': [
        'testApp',
      ],
      'mac_bundle_resources': [
        'resource.txt',
      ],
      'variables': {
        # This must *not* be set for xctest ui tests.
        'xctest_host': '',
      },
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/XCTest.framework',
        ]
      },
      'xcode_settings': {
        'WRAPPER_EXTENSION': 'xctest',
        'TEST_TARGET_NAME': 'testApp',
      },
    },
  ],
}

