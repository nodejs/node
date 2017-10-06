# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['../../gypfiles/toolchain.gypi', '../../gypfiles/features.gypi'],
  'targets': [
    {
      'target_name': 'inspector-test',
      'type': 'executable',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
        '../../src/v8.gyp:v8_libbase',
        '../../src/v8.gyp:v8',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'inspector-test.cc',
        'isolate-data.cc',
        'isolate-data.h',
        'task-runner.cc',
        'task-runner.h',
      ],
      'conditions': [
        ['v8_enable_i18n_support==1', {
          'dependencies': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'inspector-test_run',
          'type': 'none',
          'dependencies': [
            'inspector-test',
          ],
          'includes': [
            '../../gypfiles/isolate.gypi',
          ],
          'sources': [
            'inspector.isolate',
          ],
        },
      ],
    }],
  ],
}
