# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test that the case where an action is only specified under a conditional is
# evaluated appropriately.
{
  'targets': [
    {
      'target_name': 'extension_does_not_match_sources_and_no_action',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'file1.in',
        'file2.in',
      ],
      'rules': [
        {
          'rule_name': 'assemble',
          'extension': 'asm',
          'outputs': [
            '<(RULE_INPUT_ROOT).fail',
          ],
          'conditions': [
            # Always fails.
            [ '"true"=="false"', {
              'action': [
                'python', '../copy-file.py', '<(RULE_INPUT_PATH)', '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'test_rule',
            }],
          ],
        },
      ],
    },
  ],
}
