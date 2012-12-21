# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # This first target generates a header.
    {
      'target_name': 'generate_header',
      'type': 'none',
      'msvs_cygwin_shell': '0',
      'actions': [
        {
          'action_name': 'generate header',
          'inputs': [],
          'outputs': ['<(SHARED_INTERMEDIATE_DIR)/generated/header.h'],
          'action': [
            'python', '-c', 'open(<(_outputs), "w")'
          ]
        },
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },

    # This intermediate target does nothing other than pull in a
    # dependency on the above generated target.
    {
      'target_name': 'chain',
      'type': 'none',
      'dependencies': [
        'generate_header',
      ],
    },

    # This final target is:
    # - a static library (so gyp doesn't transitively pull in dependencies);
    # - that relies on the generated file two dependencies away.
    {
      'target_name': 'chained',
      'type': 'static_library',
      'dependencies': [
        'chain',
      ],
      'sources': [
        'chained.c',
      ],
    },
  ],
}
