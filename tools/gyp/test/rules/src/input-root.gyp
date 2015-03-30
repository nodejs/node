# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test',
      'type': 'executable',
      'sources': [ 'somefile.ext', ],
      'rules': [{
        'rule_name': 'rule',
        'extension': 'ext',
        'inputs': [ 'rule.py', ],
        'outputs': [ '<(RULE_INPUT_ROOT).cc', ],
        'action': [ 'python', 'rule.py', '<(RULE_INPUT_ROOT)', ],
        'message': 'Processing <(RULE_INPUT_PATH)',
        'process_outputs_as_sources': 1,
        # Allows the test to run without hermetic cygwin on windows.
        'msvs_cygwin_shell': 0,
      }],
    },
  ],
}
