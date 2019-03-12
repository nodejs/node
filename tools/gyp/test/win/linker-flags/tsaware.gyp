# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_tsaware_no',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'TerminalServerAware': '1',
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_tsaware_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'TerminalServerAware': '2',
        },
      },
      'sources': ['hello.cc'],
    },
  ]
}
