# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    # This test shouldn't ever actually need to execute its rules: there's no
    # command line that generates any output anyway. However, there's something
    # slightly broken in either ninja or (maybe more likely?) on the win32 VM
    # gypbots that breaks dependency checking and causes this rule to want to
    # run. When it does run, the cygwin path is wrong, so the do-nothing step
    # fails.
    # TODO: Investigate and fix whatever's actually failing and remove this.
    'msvs_cygwin_dirs': ['../../../../../../<(DEPTH)/third_party/cygwin'],
  },
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
