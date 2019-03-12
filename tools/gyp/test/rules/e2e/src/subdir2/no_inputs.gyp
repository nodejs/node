# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test that the case where there are no inputs (other than the
# file the rule applies to).
{
  'targets': [
    {
      'target_name': 'files_no_input',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'file1.in',
        'file2.in',
      ],
      'rules': [
        {
          'rule_name': 'copy_file2',
          'extension': 'in',
          'outputs': [
            '<(RULE_INPUT_ROOT).out2',
          ],
          'action': [
            'python', '../copy-file.py', '<(RULE_INPUT_PATH)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
