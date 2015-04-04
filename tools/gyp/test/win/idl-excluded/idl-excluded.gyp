# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'exclude_with_action',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'actions': [{
        'action_name': 'copy_action',
        'inputs': [
          'copy-file.py',
          'bad.idl',
        ],
        'outputs': [
          '<(INTERMEDIATE_DIR)/bad.idl',
        ],
        'action': [
          'python', '<@(_inputs)', '<@(_outputs)',
        ],
      }],
    },
    {
      'target_name': 'exclude_with_rule',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'bad.idl',
      ],
      'rules': [{
        'rule_name': 'copy_rule',
        'extension': 'idl',
        'inputs': [
          'copy-file.py',
        ],
        'outputs': [
          '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).idl',
        ],
        'action': [
          'python', '<@(_inputs)', '<(RULE_INPUT_PATH)', '<@(_outputs)',
        ],
      }],
    },
    {
      'target_name': 'program',
      'type': 'executable',
      'sources': [
        'program.cc',
      ],
      'dependencies': [
        'exclude_with_action',
        'exclude_with_rule',
      ],
    },
  ],
}
