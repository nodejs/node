# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test',
      'type': 'none',
      'sources': [
        'dummy.foo',
        'dummy.bar',
      ],
      'rules': [
        {
          'rule_name': 'test_stdout',
          'extension': 'foo',
          'message': 'testing stdout',
          'msvs_cygwin_shell': 0,
          'inputs': [
            'rule_stdout.py',
          ],
          'outputs': [
            'dummy.foo_output',
          ],
          'action': [
            'python',
            'rule_stdout.py',
            '<(RULE_INPUT_PATH)',
          ],
        },
        {
          'rule_name': 'test_stderr',
          'extension': 'bar',
          'message': 'testing stderr',
          'msvs_cygwin_shell': 0,
          'inputs': [
            'rule_stderr.py',
          ],
          'outputs': [
            'dummy.bar_output',
          ],
          'action': [
            'python',
            'rule_stderr.py',
            '<(RULE_INPUT_PATH)',
          ],
        },
      ],
    },
  ],
}
