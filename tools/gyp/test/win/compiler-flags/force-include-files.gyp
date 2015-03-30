# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_force_include_files',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'ForcedIncludeFiles': ['string', 'vector', 'list'],
        },
      },
      'sources': [
        'force-include-files.cc',
      ],
    },
    {
      'target_name': 'test_force_include_with_precompiled',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'ForcedIncludeFiles': ['string'],
        },
      },
      'msvs_precompiled_header': 'stdio.h',
      'msvs_precompiled_source': 'precomp.cc',
      'msvs_disabled_warnings': [ 4530, ],
      'sources': [
        'force-include-files-with-precompiled.cc',
        'precomp.cc',
      ],
    },
  ],
}
