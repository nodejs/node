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
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generate_headers',
          'inputs': [
            'emit.py'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/generated.h'
          ],
          'action': [
            'python',
            'emit.py',
            '<(SHARED_INTERMEDIATE_DIR)/generated.h',
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
      'type': 'static_library',
      'sources': [
        'b.c',
        'b.h',
      ],
      'dependencies': [
        'a',
      ],
      'export_dependent_settings': [
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
    },
    {
      'target_name': 'd',
      'type': 'static_library',
      'sources': [
        'd.c',
      ],
      'dependencies': [
        'c',
      ],
    }
  ],
}
