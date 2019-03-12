# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'null_input',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'actions': [
        {
          'action_name': 'generate_main',
          'process_outputs_as_sources': 1,
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/main.c',
          ],
          'action': [
            # TODO:  we can't just use <(_outputs) here?!
            'python', 'generate_main.py', '<(INTERMEDIATE_DIR)/main.c',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
