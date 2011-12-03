# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'files',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'file1.in0',
        'file2.in0',
        'file3.in1',
        'file4.in1',
      ],
      'rules': [
        {
          'rule_name': 'copy_file_0',
          'extension': 'in0',
          'inputs': [
            '../copy-file.py',
          ],
          'outputs': [
            'rules-out/<(RULE_INPUT_ROOT).out',
          ],
          'action': [
            'python', '<(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 0,
        },
        {
          'rule_name': 'copy_file_1',
          'extension': 'in1',
          'inputs': [
            '../copy-file.py',
          ],
          'outputs': [
            'rules-out/<(RULE_INPUT_ROOT).out',
          ],
          'action': [
            'python', '<(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
