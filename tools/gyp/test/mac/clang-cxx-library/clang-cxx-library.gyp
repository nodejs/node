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
      'target_name': 'libc++',
      'type': 'executable',
      'sources': [ 'libc++.cc', ],
      'xcode_settings': {
        'CC': 'clang',
        # libc++ requires OS X 10.7+.
        'MACOSX_DEPLOYMENT_TARGET': '10.7',
        'CLANG_CXX_LIBRARY': 'libc++',
      },
    },
    {
      'target_name': 'libstdc++',
      'type': 'executable',
      'sources': [ 'libstdc++.cc', ],
      'xcode_settings': {
        'CC': 'clang',
        'CLANG_CXX_LIBRARY': 'libstdc++',
      },
    },
  ],
}

