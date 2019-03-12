# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
  ],
  'targets': [
    # This target will not be built, but is here so that ninja Xcode emulation
    # understand this is a multi-platform (ios + mac) build.
    {
      'target_name': 'TestDummy',
      'product_name': 'TestDummy',
      'toolsets': ['target'],
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'tool_main.cc',
      ],
      'xcode_settings': {
        'SDKROOT': 'iphonesimulator',  # -isysroot
        'TARGETED_DEVICE_FAMILY': '1,2',
        'IPHONEOS_DEPLOYMENT_TARGET': '7.0',
      },
    },
    {
      'target_name': 'TestHost',
      'product_name': 'TestHost',
      'toolsets': ['host'],
      'type': 'executable',
      'mac_bundle': 0,
      'sources': [
        'tool_main.cc',
      ],
      'xcode_settings': {
        'SDKROOT': 'macosx',
        'ARCHS': [
          '$(ARCHS_STANDARD)',
          'x86_64',
        ],
        'VALID_ARCHS': [
          'x86_64',
        ],
      }
    }
  ],
}
