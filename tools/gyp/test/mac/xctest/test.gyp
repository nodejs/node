# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'classes',
      'type': 'static_library',
      'sources': [
        'MyClass.h',
        'MyClass.m',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        ],
      },
    },
    {
      'target_name': 'tests',
      'type': 'loadable_module',
      'mac_xctest_bundle': 1,
      'sources': [
        'TestCase.m',
      ],
      'dependencies': [
        'classes',
      ],
      'mac_bundle_resources': [
        'resource.txt',
      ],
      'xcode_settings': {
        'WRAPPER_EXTENSION': 'xctest',
        'FRAMEWORK_SEARCH_PATHS': [
          '$(inherited)',
          '$(DEVELOPER_FRAMEWORKS_DIR)',
        ],
        'OTHER_LDFLAGS': [
          '$(inherited)',
          '-ObjC',
        ],
      },
    },
  ],
}

