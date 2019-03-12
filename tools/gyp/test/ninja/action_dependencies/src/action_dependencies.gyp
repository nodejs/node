# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'a',
      'type': 'static_library',
      'sources': [
        'a.c',
        'a.h',
      ],
      'actions': [
        {
          'action_name': 'generate_headers',
          'inputs': [
            'emit.py'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/a/generated.h'
          ],
          'action': [
            'python',
            'emit.py',
            '<(SHARED_INTERMEDIATE_DIR)/a/generated.h',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
    {
      'target_name': 'b',
      'type': 'executable',
      'sources': [
        'b.c',
        'b.h',
      ],
      'dependencies': [
        'a',
      ],
    },
    {
      'target_name': 'c',
      'type': 'static_library',
      'sources': [
        'c.c',
        'c.h',
      ],
      'dependencies': [
        'b',
      ],
      'actions': [
        {
          'action_name': 'generate_headers',
          'inputs': [
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/c/generated.h'
          ],
          'action': [
            '<(PRODUCT_DIR)/b',
            '<(SHARED_INTERMEDIATE_DIR)/c/generated.h',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ],
}
