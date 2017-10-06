# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['../../gypfiles/toolchain.gypi', '../../gypfiles/features.gypi'],
  'targets': [
    {
      'target_name': 'mkgrokdump',
      'type': 'executable',
      'dependencies': [
        '../../src/v8.gyp:v8',
        '../../src/v8.gyp:v8_libbase',
        '../../src/v8.gyp:v8_libplatform',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'mkgrokdump.cc',
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mkgrokdump_run',
          'type': 'none',
          'dependencies': [
            'mkgrokdump',
          ],
          'includes': [
            '../../gypfiles/isolate.gypi',
          ],
          'sources': [
            'mkgrokdump.isolate',
          ],
        },
      ],
    }],
  ],
}
