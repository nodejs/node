# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test-floating-point-model-default',
      'type': 'executable',
      'sources': ['floating-point-model-precise.cc'],
    },
    {
      'target_name': 'test-floating-point-model-precise',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'FloatingPointModel': '0'
        }
      },
      'sources': ['floating-point-model-precise.cc'],
    },
    {
      'target_name': 'test-floating-point-model-strict',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'FloatingPointModel': '1'
        }
      },
      'sources': ['floating-point-model-strict.cc'],
    },
    {
      'target_name': 'test-floating-point-model-fast',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'FloatingPointModel': '2'
        }
      },
      'sources': ['floating-point-model-fast.cc'],
    },
  ]
}
