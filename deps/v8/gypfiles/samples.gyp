# Copyright 2012 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
    'v8_enable_i18n_support%': 1,
    'v8_toolset_for_shell%': 'target',
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'target_defaults': {
    'type': 'executable',
    'dependencies': [
      'v8.gyp:v8',
      'v8.gyp:v8_libbase',
      'v8.gyp:v8_libplatform',
    ],
    'include_dirs': [
      '..',
    ],
    'conditions': [
      ['v8_enable_i18n_support==1', {
        'dependencies': [
          '<(icu_gyp_path):icui18n',
          '<(icu_gyp_path):icuuc',
        ],
      }],
      ['OS=="win" and v8_enable_i18n_support==1', {
        'dependencies': [
          '<(icu_gyp_path):icudata',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'v8_shell',
      'sources': [
        '../samples/shell.cc',
      ],
      'conditions': [
        [ 'want_separate_host_toolset==1', {
          'toolsets': [ '<(v8_toolset_for_shell)', ],
        }],
      ],
    },
    {
      'target_name': 'hello-world',
      'sources': [
        '../samples/hello-world.cc',
      ],
    },
    {
      'target_name': 'process',
      'sources': [
        '../samples/process.cc',
      ],
    },
  ],
}
