# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_cdecl',
      'type': 'loadable_module',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CallingConvention': 0,
        },
      },
      'sources': [
        'calling-convention.cc',
        'calling-convention-cdecl.def',
      ],
    },
    {
      'target_name': 'test_fastcall',
      'type': 'loadable_module',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CallingConvention': 1,
        },
      },
      'sources': [
        'calling-convention.cc',
        'calling-convention-fastcall.def',
      ],
    },
    {
      'target_name': 'test_stdcall',
      'type': 'loadable_module',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CallingConvention': 2,
        },
      },
      'sources': [
        'calling-convention.cc',
        'calling-convention-stdcall.def',
      ],
    },
  ],
  'conditions': [
    ['MSVS_VERSION[0:4]>="2013"', {
      'targets': [
        {
          'target_name': 'test_vectorcall',
          'type': 'loadable_module',
          'msvs_settings': {
            'VCCLCompilerTool': {
              'CallingConvention': 3,
            },
          },
          'sources': [
            'calling-convention.cc',
            'calling-convention-vectorcall.def',
          ],
        },
      ],
    }],
  ],
}
