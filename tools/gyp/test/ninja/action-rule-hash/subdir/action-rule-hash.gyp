# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'sources': [
        '<(INTERMEDIATE_DIR)/main.cc',
      ],
      'actions': [
        {
          'action_name': 'emit_main_cc',
          'inputs': ['emit.py'],
          'outputs': ['<(INTERMEDIATE_DIR)/main.cc'],
          'action': [
            'python',
            'emit.py',
            '<(INTERMEDIATE_DIR)/main.cc',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
