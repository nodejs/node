# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test that the case where there are no inputs (other than the
# file the rule applies to).
{
  'target_defaults': {
    'msvs_cygwin_dirs': ['../../../../../../<(DEPTH)/third_party/cygwin'],
  },
  'targets': [
    {
      'target_name': 'external_rules',
      'type': 'none',
      'sources': [
        'file1.in',
        'file2.in',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'cygwin',
          ],
        }],
      ],
      'rules': [
        {
          'rule_name': 'copy_file',
          'extension': 'in',
          'msvs_external_rule': 1,
          'outputs': [
            '<(RULE_INPUT_ROOT).external_rules.out',
          ],
          'action': [
            'python', '../copy-file.py', '<(RULE_INPUT_PATH)', '<@(_outputs)',
          ],
        },
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'cygwin',
          'type': 'none',
          'actions': [
            {
              'action_name': 'setup_mount',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '../../../../../../<(DEPTH)/third_party/cygwin/setup_mount.bat',
              ],
              # Visual Studio requires an output file, or else the
              # custom build step won't run.
              'outputs': [
                '<(INTERMEDIATE_DIR)/_always_run_setup_mount.marker',
              ],
              'action': ['<@(_inputs)'],
            },
          ],
        },
      ],
    }],
  ],
}
