# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'generate_header',
      'type': 'none',
      'actions': [
        {
          'inputs': [ ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/MyHeader.h',
          ],
          'action_name': 'generate header',
          'action': ['python', './action.py',
                     '<(SHARED_INTERMEDIATE_DIR)/MyHeader.h', 'foobar output' ],
        },
      ],
      'msvs_cygwin_shell': 0,
    },
    {
      'target_name': 'program',
      'type': 'executable',
      'dependencies': [
        'generate_header',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [ 'main.cc' ],
    },
  ],
}
