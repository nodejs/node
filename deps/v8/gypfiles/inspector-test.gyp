# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'targets': [
    {
      'target_name': 'inspector-test',
      'type': 'executable',
      'dependencies': [
        'v8.gyp:v8_libplatform',
        'v8.gyp:v8_libbase',
        'v8.gyp:v8',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/inspector/inspector-test.cc',
        '../test/inspector/isolate-data.cc',
        '../test/inspector/isolate-data.h',
        '../test/inspector/task-runner.cc',
        '../test/inspector/task-runner.h',
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
}
