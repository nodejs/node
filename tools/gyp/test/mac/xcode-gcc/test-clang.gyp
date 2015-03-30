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
      'target_name': 'aliasing_yes',
      'type': 'executable',
      'sources': [ 'aliasing.cc', ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'GCC_STRICT_ALIASING': 'YES',
        'GCC_OPTIMIZATION_LEVEL': 2,
      },
    },
    {
      'target_name': 'aliasing_no',
      'type': 'executable',
      'sources': [ 'aliasing.cc', ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'GCC_STRICT_ALIASING': 'NO',
        'GCC_OPTIMIZATION_LEVEL': 2,
      },
    },
    {
      'target_name': 'aliasing_default',
      'type': 'executable',
      'sources': [ 'aliasing.cc', ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'GCC_OPTIMIZATION_LEVEL': 2,
      },
    },
  ],
}

