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
        '<(V8_ROOT)/third_party/ittapi/src/ittnotify/ittnotify_config.h',
        '<(V8_ROOT)/third_party/ittapi/src/ittnotify/ittnotify_types.h',
        '<(V8_ROOT)/third_party/ittapi/src/ittnotify/jitprofiling.c',
        '<(V8_ROOT)/third_party/ittapi/include/jitprofiling.h',
        '<(V8_ROOT)/src/third_party/vtune/v8-vtune.h',
        '<(V8_ROOT)/src/third_party/vtune/vtune-jit.cc',
        '<(V8_ROOT)/src/third_party/vtune/vtune-jit.h',
      ],
      'include_dirs': [
        '<(V8_ROOT)/third_party/ittapi/include',
        '<(V8_ROOT)/third_party/ittapi/src/ittnotify',
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
