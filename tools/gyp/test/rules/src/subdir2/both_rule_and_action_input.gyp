# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tests that if a rule input is also an action input, both the rule and action
# are executed
{
  'targets': [
    {
      'target_name': 'files_both_rule_and_action_input',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'sources': [
        'program.c',
        'file1.in',
        'file2.in',
      ],
      'rules': [
        {
          'rule_name': 'copy_file',
          'extension': 'in',
          'inputs': [
            '../copy-file.py',
          ],
          'outputs': [
            '<(RULE_INPUT_ROOT).out4',
          ],
          'action': [
            'python', '<(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)',
          ],
        },
      ],
      'actions': [
        {
          'action_name': 'copy_file1_in',
          'inputs': [
            '../copy-file.py',
            'file1.in',
          ],
          'outputs': [
            'file1.copy',
          ],
          'action': [
            'python', '<@(_inputs)', '<(_outputs)'
          ],
        },
      ],
    },
  ],
}
