# Copyright 2012 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'V8_ROOT': '../../deps/v8',
    'v8_code': 1,
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'targets': [
    {
      'target_name': 'v8_vtune',
      'type': 'static_library',
      'sources': [
        '<(V8_ROOT)/third_party/ittapi/src/ittnotify/jitprofiling.c',
        '<(V8_ROOT)/third_party/vtune/vtune-jit.cc',
      ],
      'include_dirs': [
        '<(V8_ROOT)/third_party/ittapi/include',
        '<(V8_ROOT)/third_party/ittapi/src/ittnotify',
        '<(V8_ROOT)/include',
        '<(V8_ROOT)',
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
