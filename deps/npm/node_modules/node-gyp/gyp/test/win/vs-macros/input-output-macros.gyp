# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_expansions',
      'msvs_cygwin_shell': 0,
      'type': 'none',
      'rules': [
        {
          'rule_name': 'generate_file',
          'extension': 'blah',
          'inputs': [
            '<(RULE_INPUT_PATH)',
            'do_stuff.py',
          ],
          'outputs': [
            '$(OutDir)\\<(RULE_INPUT_NAME).something',
          ],
          'action': ['python',
                     'do_stuff.py',
                     '<(RULE_INPUT_PATH)',
                     '$(OutDir)\\<(RULE_INPUT_NAME).something',],
        },
      ],
      'sources': [
        'stuff.blah',
      ],
    },
  ]
}
