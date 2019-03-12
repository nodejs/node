# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # the test driver switches this flag when testing external builder
    'use_external_builder%': 0,
  },
  'targets': [
    {
      'target_name': 'external',
      'type': 'executable',
      'sources': [
        'hello.cpp',
        'hello.z',
      ],
      'rules': [
        {
          'rule_name': 'test_rule',
          'extension': 'z',
          'outputs': [
            'msbuild_rule.out',
          ],
          'action': [
            'python',
            'msbuild_rule.py',
            '<(RULE_INPUT_PATH)',
            'a', 'b', 'c',
          ],
          'msvs_cygwin_shell': 0,
        },
      ],
      'actions': [
        {
          'action_name': 'test action',
          'inputs': [
            'msbuild_action.py',
          ],
          'outputs': [
            'msbuild_action.out',
          ],
          'action': [
            'python',
            '<@(_inputs)',
            'x', 'y', 'z',
          ],
          'msvs_cygwin_shell': 0,
        },
      ],
      'conditions': [
        ['use_external_builder==1', {
          'msvs_external_builder': 'test',
          'msvs_external_builder_build_cmd': [
            'python',
            'external_builder.py',
            'build', '1', '2', '3',
          ],
          'msvs_external_builder_clean_cmd': [
            'python',
            'external_builder.py',
            'clean', '4', '5',
          ],
        }],
      ],
    },
  ],
}
