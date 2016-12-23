# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
    'inspector_protocol_sources': [
      'inspector-impl.cc',
      'inspector-impl.h',
      'inspector-test.cc',
      'task-runner.cc',
      'task-runner.h',
    ],
  },
  'includes': ['../../gypfiles/toolchain.gypi', '../../gypfiles/features.gypi'],
  'targets': [
    {
      'target_name': 'inspector-test',
      'type': 'executable',
      'dependencies': [
        '../../src/v8.gyp:v8_libplatform',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        '<@(inspector_protocol_sources)',
      ],
      'conditions': [
        ['component=="shared_library"', {
          # inspector-test can't be built against a shared library, so we
          # need to depend on the underlying static target in that case.
          'dependencies': ['../../src/v8.gyp:v8_maybe_snapshot'],
        }, {
          'dependencies': ['../../src/v8.gyp:v8'],
        }],
      ],
    },
  ],
}
