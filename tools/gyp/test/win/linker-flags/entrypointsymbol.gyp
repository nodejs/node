# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_ok',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'EntryPointSymbol': 'MainEntryPoint',
        }
      },
      'sources': ['entrypointsymbol.cc'],
    },
    {
      'target_name': 'test_fail',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'EntryPointSymbol': 'MainEntryPoint',
        }
      },
      'sources': ['hello.cc'],
    },
  ]
}
