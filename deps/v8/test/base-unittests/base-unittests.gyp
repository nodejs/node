# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['../../build/toolchain.gypi', '../../build/features.gypi'],
  'targets': [
    {
      'target_name': 'base-unittests',
      'type': 'executable',
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
        '../../tools/gyp/v8.gyp:v8_libbase',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'cpu-unittest.cc',
        'platform/condition-variable-unittest.cc',
        'platform/mutex-unittest.cc',
        'platform/platform-unittest.cc',
        'platform/time-unittest.cc',
        'utils/random-number-generator-unittest.cc',
      ],
      'conditions': [
        ['os_posix == 1', {
          # TODO(svenpanne): This is a temporary work-around to fix the warnings
          # that show up because we use -std=gnu++0x instead of -std=c++11.
          'cflags!': [
            '-pedantic',
          ],
        }],
      ],
    },
  ],
}
