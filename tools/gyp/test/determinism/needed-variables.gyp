# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test',
      'type': 'executable',
      'sources': ['rule.ext'],
      'rules': [{
        'rule_name': 'rule',
        'extension': 'ext',
        'inputs': [ 'rule.py', ],
        'action': [
          'python',
          'rule.py',
          '<(RULE_INPUT_ROOT)',
          '<(RULE_INPUT_EXT)',
          '<(RULE_INPUT_DIRNAME)',
          '<(RULE_INPUT_NAME)',
          '<(RULE_INPUT_PATH)',
        ],
        'outputs': [ 'hello_world.txt' ],
        'sources': ['rule.ext'],
        'message': 'Processing <(RULE_INPUT_PATH)',
        'process_outputs_as_sources': 1,
        # Allows the test to run without hermetic cygwin on windows.
        'msvs_cygwin_shell': 0,
      }],
    },
  ],
}
