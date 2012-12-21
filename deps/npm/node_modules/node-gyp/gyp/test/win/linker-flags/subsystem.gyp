# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_console_ok',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_console_fail',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1'
        }
      },
      'sources': ['subsystem-windows.cc'],
    },
    {
      'target_name': 'test_windows_ok',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2'
        }
      },
      'sources': ['subsystem-windows.cc'],
    },
    {
      'target_name': 'test_windows_fail',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2'
        }
      },
      'sources': ['hello.cc'],
    },
  ]
}
