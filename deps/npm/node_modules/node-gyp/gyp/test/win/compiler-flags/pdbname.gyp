# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_pdbname',
      'type': 'executable',
      'sources': [
        'hello.cc',
        'pdbname.cc',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
      },
    },
  ]
}
