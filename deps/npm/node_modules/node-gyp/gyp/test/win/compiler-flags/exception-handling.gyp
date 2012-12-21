# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Optimization disabled so that the exception-causing code is not removed
    # (divide by zero was getting optimized away in VS2010).
    {
      'target_name': 'test_eh_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'ExceptionHandling': '0',
          'WarnAsError': 'true',
          'Optimization': '0',
        }
      },
      'sources': ['exception-handling-on.cc'],
    },
    {
      'target_name': 'test_eh_s',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'ExceptionHandling': '1',
          'WarnAsError': 'true',
          'Optimization': '0',
        }
      },
      'sources': ['exception-handling-on.cc'],
    },
    {
      'target_name': 'test_eh_a',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'ExceptionHandling': '2',
          'WarnAsError': 'true',
          'Optimization': '0',
        }
      },
      'sources': ['exception-handling-on.cc'],
    },
  ]
}
