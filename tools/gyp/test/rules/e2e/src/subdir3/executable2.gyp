# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This one tests that rules are properly written if extensions are different
# between the target's sources (program.c) and the generated files
# (function3.cc)

{
  'targets': [
    {
      'target_name': 'program2',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'sources': [
        'program.c',
        'function3.in',
      ],
      'rules': [
        {
          'rule_name': 'copy_file',
          'extension': 'in',
          'inputs': [
            '../copy-file.py',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).cc',
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
