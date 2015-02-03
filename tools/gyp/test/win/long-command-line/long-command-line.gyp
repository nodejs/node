# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'longexe',
      'type': 'executable',
      'msvs_settings': {
        # Use this as a simple way to get a long command.
        'VCCLCompilerTool': {
          'AdditionalOptions': '/nologo ' * 8000,
        },
        'VCLinkerTool': {
          'AdditionalOptions': '/nologo ' * 8000,
        },
      },
      'sources': [
        'hello.cc',
      ],
    },
    {
      'target_name': 'longlib',
      'type': 'static_library',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'AdditionalOptions': '/nologo ' * 8000,
        },
        'VCLibrarianTool': {
          'AdditionalOptions': '/nologo ' * 8000,
        },
      },
      'sources': [
        'function.cc',
      ],
    },
    {
      'target_name': 'longdll',
      'type': 'shared_library',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'AdditionalOptions': '/nologo ' * 8000,
        },
        'VCLinkerTool': {
          'AdditionalOptions': '/nologo ' * 8000,
        },
      },
      'sources': [
        'hello.cc',
      ],
    },
  ]
}
