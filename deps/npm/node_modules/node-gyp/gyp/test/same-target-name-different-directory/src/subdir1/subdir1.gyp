# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'target',
      'type': 'none',
      'actions': [
        {
          'action_name': 'action1',
          'inputs': [],
          'outputs': [
            'action1.txt',
          ],
          'action': [
            'python', '../touch.py', '<(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
    {
      'target_name': 'target_same_action_name',
      'type': 'none',
      'actions': [
        {
          'action_name': 'action',
          'inputs': [],
          'outputs': [
            'action.txt',
          ],
          'action': [
            'python', '../touch.py', '<(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
    {
      'target_name': 'target_same_rule_name',
      'type': 'none',
      'sources': [
        '../touch.py'
      ],
      'rules': [
        {
          'rule_name': 'rule',
          'extension': 'py',
          'inputs': [],
          'outputs': [
            'rule.txt',
          ],
          'action': [
            'python', '../touch.py', '<(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
