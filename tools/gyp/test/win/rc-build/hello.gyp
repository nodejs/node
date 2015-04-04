# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'with_resources',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
        'VCResourceCompilerTool': {
          'Culture' : '1033',
        },
      },
      'sources': [
        'hello.cpp',
        'hello.rc',
      ],
      'libraries': [
        'kernel32.lib',
        'user32.lib',
      ],
    },
    {
      'target_name': 'with_resources_subdir',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
        'VCResourceCompilerTool': {
          'Culture' : '1033',
        },
      },
      'sources': [
        'hello.cpp',
        'subdir/hello2.rc',
      ],
      'libraries': [
        'kernel32.lib',
        'user32.lib',
      ],
    },
    {
      'target_name': 'with_include_subdir',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
        'VCResourceCompilerTool': {
          'Culture' : '1033',
        },
      },
      'resource_include_dirs': [
        '$(ProjectDir)\\subdir',
      ],
      'sources': [
        'hello.cpp',
        'hello3.rc',
      ],
      'libraries': [
        'kernel32.lib',
        'user32.lib',
      ],
    },
    {
      'target_name': 'resource_only_dll',
      'type': 'shared_library',
      'msvs_settings': {
        'VCLinkerTool': {
          'ResourceOnlyDLL': 'true',
        },
      },
      'sources': [
        'hello.rc',
      ],
    },
  ],
}
