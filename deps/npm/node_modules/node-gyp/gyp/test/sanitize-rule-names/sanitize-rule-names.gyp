# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 's_test',
      'type': 'executable',
      'rules': [
        {
          # Make sure this rule name doesn't cause an invalid ninja file.
          'rule_name': 'rule name with odd characters ()/',
          'extension': 'S',
          'outputs': ['outfile'],
          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'action': ['python', 'script.py', '<(RULE_INPUT_PATH)', 'outfile'],
        },
      ],
      'sources': [
        'blah.S',
        'hello.cc',
      ],
    },
  ],
}
