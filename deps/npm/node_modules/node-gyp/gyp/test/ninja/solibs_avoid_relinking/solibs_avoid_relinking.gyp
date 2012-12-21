# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'a',
      'type': 'shared_library',
      'sources': [ 'solib.cc' ],
      # Incremental linking enabled so that .lib timestamp is maintained when
      # exports are unchanged.
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '2',
        }
      },
    },
    {
      'target_name': 'b',
      'type': 'executable',
      'sources': [ 'main.cc' ],
      'dependencies': [ 'a' ],
      'msvs_settings': {
        'VCLinkerTool': {
          'LinkIncremental': '2',
        }
      },
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'target_defaults': {
        'cflags': ['-fPIC'],
      },
    }],
  ],
}
