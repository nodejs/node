# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'target2',
      'type': 'none',
      'sources': [
        '../touch.py'
      ],
      'rules': [
        {
          'rule_name': 'rule2',
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
