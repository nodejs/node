# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
    # Enable support for Intel VTune. Supported on ia32/x64 only
    'v8_enable_vtunejit%': 0,
    'v8_enable_i18n_support%': 1,
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'targets': [
    {
      'target_name': 'd8',
      'type': 'executable',
      'dependencies': [
        'v8.gyp:v8',
        'v8.gyp:v8_libbase',
        'v8.gyp:v8_libplatform',
      ],
      # Generated source files need this explicitly:
      'include_dirs+': [
        '..',
        '<(DEPTH)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/d8-js.cc',
        '../src/async-hooks-wrapper.cc',
        '../src/async-hooks-wrapper.h',
        '../src/d8-console.cc',
        '../src/d8-console.h',
        '../src/d8.cc',
        '../src/d8.h',
      ],
      'conditions': [
        [ 'want_separate_host_toolset==1', {
          'toolsets': [ 'target', ],
          'dependencies': [
            'd8_js2c#host',
          ],
        }, {
          'dependencies': [
            'd8_js2c',
          ],
        }],
        ['(OS=="linux" or OS=="mac" or OS=="freebsd" or OS=="netbsd" \
           or OS=="openbsd" or OS=="solaris" or OS=="android" \
           or OS=="qnx" or OS=="aix")', {
             'sources': [ '../src/d8-posix.cc', ]
           }],
        [ 'OS=="win"', {
          'sources': [ '../src/d8-windows.cc', ]
        }],
        [ 'component!="shared_library"', {
          'conditions': [
            [ 'v8_postmortem_support=="true"', {
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                   '-Wl,-force_load,<(PRODUCT_DIR)/libv8_base.a'
                ],
              },
            }],
          ],
        }],
        ['v8_enable_vtunejit==1', {
          'dependencies': [
            'v8vtune.gyp:v8_vtune',
          ],
        }],
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
    {
      'target_name': 'd8_js2c',
      'type': 'none',
      'variables': {
        'js_files': [
          '../src/d8.js',
          '../src/js/macros.py',
        ],
      },
      'conditions': [
        [ 'want_separate_host_toolset==1', {
          'toolsets': ['host'],
        }, {
          'toolsets': ['target'],
        }]
      ],
      'actions': [
        {
          'action_name': 'd8_js2c',
          'inputs': [
            '../tools/js2c.py',
            '<@(js_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/d8-js.cc',
          ],
          'action': [
            'python',
            '../tools/js2c.py',
            '<@(_outputs)',
            'D8',
            '<@(js_files)'
          ],
        },
      ],
    },
  ],
}
