# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_default',
      'type': 'executable',
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_set_reserved_size',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'StackReserveSize': 2097152,  # 2MB
        }
      },
    },
    {
      'target_name': 'test_set_commit_size',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'StackCommitSize': 8192,  # 8KB
        }
      },
    },
    {
      'target_name': 'test_set_both',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'StackReserveSize': 2097152,  # 2MB
          'StackCommitSize': 8192,  # 8KB
        }
      },
    },
  ]
}
