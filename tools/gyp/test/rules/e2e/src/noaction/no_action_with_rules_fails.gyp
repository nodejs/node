# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test the case where there's no action but there are input rules that should
# be processed results in a gyp failure.
{
  'targets': [
    {
      'target_name': 'extension_does_match_sources_but_no_action',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'file1.in',
      ],
      'rules': [
        {
          'rule_name': 'assembled',
          'extension': 'in',
          'outputs': [
            '<(RULE_INPUT_ROOT).in',
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
