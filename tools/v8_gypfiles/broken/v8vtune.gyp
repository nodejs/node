# Copyright 2012 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'targets': [
    {
      'target_name': 'v8_vtune',
      'type': 'static_library',
      'dependencies': [
        'v8.gyp:v8',
      ],
      'sources': [
        '../src/third_party/vtune/ittnotify_config.h',
        '../src/third_party/vtune/ittnotify_types.h',
        '../src/third_party/vtune/jitprofiling.cc',
        '../src/third_party/vtune/jitprofiling.h',
        '../src/third_party/vtune/v8-vtune.h',
        '../src/third_party/vtune/vtune-jit.cc',
        '../src/third_party/vtune/vtune-jit.h',
      ],
      'direct_dependent_settings': {
        'defines': ['ENABLE_VTUNE_JIT_INTERFACE',],
        'conditions': [
          ['OS != "win"', {
            'libraries': ['-ldl',],
          }],
        ],
      },
    },
  ],
}
