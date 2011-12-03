# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'all_rule_variables',
      'type': 'executable',
      'sources': [
        'subdir/test.c',
      ],
      'rules': [
        {
          'rule_name': 'rule_variable',
          'extension': 'c',
          'outputs': [
            '<(RULE_INPUT_ROOT).input_root.c',
            '<(RULE_INPUT_DIRNAME)/input_dirname.c',
            'input_path/<(RULE_INPUT_PATH)',
            'input_ext<(RULE_INPUT_EXT)',
            'input_name/<(RULE_INPUT_NAME)',
          ],
          'action': [],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
