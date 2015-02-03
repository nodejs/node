# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
    ['CXX', '/usr/bin/clang++'],
  ],

  'targets': [
    {
      'target_name': 'arc_enabled',
      'type': 'static_library',
      'sources': [
        'c-file.c',
        'cc-file.cc',
        'm-file.m',
        'mm-file.mm',
      ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'MACOSX_DEPLOYMENT_TARGET': '10.6',
        'ARCHS': [ 'x86_64' ],  # For the non-fragile objc ABI.
        'CLANG_ENABLE_OBJC_ARC': 'YES',
      },
    },

    {
      'target_name': 'arc_disabled',
      'type': 'static_library',
      'sources': [
        'c-file.c',
        'cc-file.cc',
        'm-file-no-arc.m',
        'mm-file-no-arc.mm',
      ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'MACOSX_DEPLOYMENT_TARGET': '10.6',
        'ARCHS': [ 'x86_64' ],  # For the non-fragile objc ABI.
      },
    },
  ],
}

