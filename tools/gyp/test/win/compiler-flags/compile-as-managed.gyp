# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test-compile-as-managed',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CompileAsManaged': 'true',
          'ExceptionHandling': '0' # /clr is incompatible with /EHs
        }
      },
      'sources': ['compile-as-managed.cc'],
    },
    {
      'target_name': 'test-compile-as-unmanaged',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CompileAsManaged': 'false',
        }
      },
      'sources': ['compile-as-managed.cc'],
    },
  ]
}
