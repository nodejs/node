# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_dld_none',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
        }
      },
      'sources': ['delay-load.cc'],
      'libraries': [
        'delayimp.lib',
        'shell32.lib',
      ],
    },
    {
      'target_name': 'test_dld_shell32',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'DelayLoadDLLs': ['shell32.dll']
        }
      },
      'sources': ['delay-load.cc'],
      'libraries': [
        'delayimp.lib',
        'shell32.lib',
      ],
    },
  ]
}
