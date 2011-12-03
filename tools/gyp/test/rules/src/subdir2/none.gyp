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
            '<(RULE_INPUT_ROOT).out',
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
