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
      'target_name': 'c++98',
      'type': 'executable',
      'sources': [ 'c++98.cc', ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'CLANG_CXX_LANGUAGE_STANDARD': 'c++98',
      },
    },
    {
      'target_name': 'c++11',
      'type': 'executable',
      'sources': [ 'c++11.cc', ],
      'xcode_settings': {
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'CLANG_CXX_LANGUAGE_STANDARD': 'c++0x',
      },
    },
  ],
}

