# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Have a long string so that actions will exceed xp 512 character
    # command limit on xp.
    'long_string':
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
        'abcdefghijklmnopqrstuvwxyz0123456789'
  },
  'targets': [
    {
      'target_name': 'multiple_action_target',
      'type': 'none',
      'actions': [
        {
          'action_name': 'action1',
          'inputs': [
            'copyfile.py',
            'input.txt',
          ],
          'outputs': [
            'output1.txt',
          ],
          'action': [
            'python', '<@(_inputs)', '<(_outputs)', '<(long_string)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'action2',
          'inputs': [
            'copyfile.py',
            'input.txt',
          ],
          'outputs': [
            'output2.txt',
          ],
          'action': [
            'python', '<@(_inputs)', '<(_outputs)', '<(long_string)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'action3',
          'inputs': [
            'copyfile.py',
            'input.txt',
          ],
          'outputs': [
            'output3.txt',
          ],
          'action': [
            'python', '<@(_inputs)', '<(_outputs)', '<(long_string)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'action4',
          'inputs': [
            'copyfile.py',
            'input.txt',
          ],
          'outputs': [
            'output4.txt',
          ],
          'action': [
            'python', '<@(_inputs)', '<(_outputs)', '<(long_string)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
    {
      'target_name': 'multiple_action_source_filter',
      'type': 'executable',
      'sources': [
        'main.c',
        # TODO(bradnelson): add foo.c here once this issue is fixed:
        #     http://code.google.com/p/gyp/issues/detail?id=175
      ],
      'actions': [
        {
          'action_name': 'action1',
          'inputs': [
            'foo.c',
            'filter.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/output1.c',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python', 'filter.py', 'foo', 'bar', 'foo.c', '<@(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'action2',
          'inputs': [
            'foo.c',
            'filter.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/output2.c',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python', 'filter.py', 'foo', 'car', 'foo.c', '<@(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'action3',
          'inputs': [
            'foo.c',
            'filter.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/output3.c',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python', 'filter.py', 'foo', 'dar', 'foo.c', '<@(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'action4',
          'inputs': [
            'foo.c',
            'filter.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/output4.c',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python', 'filter.py', 'foo', 'ear', 'foo.c', '<@(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
    {
      'target_name': 'multiple_dependent_target',
      'type': 'none',
      'actions': [
        {
          'action_name': 'action1',
          'inputs': [
            'copyfile.py',
            'input.txt',
          ],
          'outputs': [
            'multi1.txt',
          ],
          'action': [
            'python', '<@(_inputs)', '<(_outputs)', '<(long_string)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'action2',
          'inputs': [
            'copyfile.py',
            'input.txt',
          ],
          'outputs': [
            'multi2.txt',
          ],
          'action': [
            'python', '<@(_inputs)', '<(_outputs)', '<(long_string)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
      'dependencies': [
        'multiple_required_target',
      ],
    },
    {
      'target_name': 'multiple_required_target',
      'type': 'none',
      'actions': [
        {
          'action_name': 'multi_dep',
          'inputs': [
            'copyfile.py',
            'input.txt',
          ],
          'outputs': [
            'multi_dep.txt',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python', '<@(_inputs)', '<(_outputs)', '<(long_string)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
