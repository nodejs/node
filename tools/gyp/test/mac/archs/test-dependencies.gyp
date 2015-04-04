# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'xcode_settings': {
      'ARCHS': ['i386', 'x86_64'],
    },
  },
  'targets': [
    {
      'target_name': 'target_a',
      'type': 'static_library',
      'sources': [
        'file_a.cc',
        'file_a.h',
      ],
    },
    {
      'target_name': 'target_b',
      'type': 'static_library',
      'sources': [
        'file_b.cc',
        'file_b.h',
      ],
    },
    {
      'target_name': 'target_c_standalone_helper',
      'type': 'loadable_module',
      'hard_dependency': 1,
      'dependencies': [
        'target_a',
        'target_b',
      ],
      'sources': [
        'file_c.cc',
      ],
    },
    {
      'target_name': 'target_c_standalone',
      'type': 'none',
      'dependencies': [
        'target_c_standalone_helper',
      ],
      'actions': [
        {
          'action_name': 'Package C',
          'inputs': [],
          'outputs': [
            '<(PRODUCT_DIR)/libc_standalone.a',
          ],
          'action': [
            'touch',
            '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'target_d_standalone_helper',
      'type': 'shared_library',
      'dependencies': [
        'target_a',
        'target_b',
      ],
      'sources': [
        'file_d.cc',
      ],
    },
    {
      'target_name': 'target_d_standalone',
      'type': 'none',
      'dependencies': [
        'target_d_standalone_helper',
      ],
      'actions': [
        {
          'action_name': 'Package D',
          'inputs': [],
          'outputs': [
            '<(PRODUCT_DIR)/libd_standalone.a',
          ],
          'action': [
            'touch',
            '<@(_outputs)',
          ],
        },
      ],
    }
  ],
}
