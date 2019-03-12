# Copyright (c) 2016 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test-compile-as-winrt',
      'type': 'executable',
      'msvs_windows_sdk_version': 'v10.0',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'AdditionalUsingDirectories': ['$(VCInstallDir)vcpackages;$(WindowsSdkDir)UnionMetadata;%(AdditionalUsingDirectories)'],
          'CompileAsWinRT': 'true'
        }
      },
      'sources': ['compile-as-winrt.cc']
    }
  ]
}
