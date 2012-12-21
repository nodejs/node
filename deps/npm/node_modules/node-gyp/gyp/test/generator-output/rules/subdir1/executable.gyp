# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'sources': [
        'program.c',
        'function1.in1',
        'function2.in1',
        'define3.in0',
        'define4.in0',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'rules': [
        {
          'rule_name': 'copy_file_0',
          'extension': 'in0',
          'inputs': [
            '../copy-file.py',
          ],
          'outputs': [
            # TODO:  fix SCons and Make to support generated files not
            # in a variable-named path like <(INTERMEDIATE_DIR)
            #'<(RULE_INPUT_ROOT).c',
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).h',
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
            # TODO:  fix SCons and Make to support generated files not
            # in a variable-named path like <(INTERMEDIATE_DIR)
            #'<(RULE_INPUT_ROOT).c',
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).c',
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
