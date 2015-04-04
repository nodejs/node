# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_deps_none',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_deps_few',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalDependencies': [
            'wininet.lib',
            'ws2_32.lib',
          ]
        }
      },
      'sources': ['additional-deps.cc'],
    },
  ]
}
