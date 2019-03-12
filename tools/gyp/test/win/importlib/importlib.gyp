# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_importlib',
      'type': 'shared_library',
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '2',
        }
      },
      'sources': ['has-exports.cc'],
    },

    {
      'target_name': 'test_linkagainst',
      'type': 'executable',
      'dependencies': ['test_importlib'],
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '2',
        }
      },
      'sources': ['hello.cc'],
    },
  ]
}
