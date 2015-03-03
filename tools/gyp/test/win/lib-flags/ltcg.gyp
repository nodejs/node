# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'lib_answer',
      'type': 'static_library',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WholeProgramOptimization': 'true',  # /GL
        },
        'VCLibrarianTool': {
          'LinkTimeCodeGeneration': 'true',    # /LTCG
        },
      },
      'sources': ['answer.cc'],
    },
  ]
}
