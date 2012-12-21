# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'pull_in_all_actions',
      'type': 'none',
      'dependencies': [
        'subdir1/executable.gyp:*',
        'subdir2/none.gyp:*',
        'subdir3/null_input.gyp:*',
      ],
    },
    {
      'target_name': 'depend_on_always_run_action',
      'type': 'none',
      'dependencies': [ 'subdir1/executable.gyp:counter' ],
      'actions': [
        {
          'action_name': 'use_always_run_output',
          'inputs': [
            'subdir1/actions-out/action-counter.txt',
            'subdir1/counter.py',
          ],
          'outputs': [
            'subdir1/actions-out/action-counter_2.txt',
          ],
          'action': [
            'python', 'subdir1/counter.py', '<(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },

    # Three deps which don't finish immediately.
    # Each one has a small delay then creates a file.
    # Delays are 1.0, 1.1, and 2.0 seconds.
    {
      'target_name': 'dep_1',
      'type': 'none',
      'actions': [{
        'inputs': [ 'actions.gyp' ],
        'outputs': [ 'dep_1.txt' ],
        'action_name': 'dep_1',
        'action': [ 'python', '-c',
                    'import time; time.sleep(1); open(\'dep_1.txt\', \'w\')' ],
        # Allows the test to run without hermetic cygwin on windows.
        'msvs_cygwin_shell': 0,
      }],
    },
    {
      'target_name': 'dep_2',
      'type': 'none',
      'actions': [{
        'inputs': [ 'actions.gyp' ],
        'outputs': [ 'dep_2.txt' ],
        'action_name': 'dep_2',
        'action': [ 'python', '-c',
                    'import time; time.sleep(1.1); open(\'dep_2.txt\', \'w\')' ],
        # Allows the test to run without hermetic cygwin on windows.
        'msvs_cygwin_shell': 0,
      }],
    },
    {
      'target_name': 'dep_3',
      'type': 'none',
      'actions': [{
        'inputs': [ 'actions.gyp' ],
        'outputs': [ 'dep_3.txt' ],
        'action_name': 'dep_3',
        'action': [ 'python', '-c',
                    'import time; time.sleep(2.0); open(\'dep_3.txt\', \'w\')' ],
        # Allows the test to run without hermetic cygwin on windows.
        'msvs_cygwin_shell': 0,
      }],
    },

    # An action which assumes the deps have completed.
    # Does NOT list the output files of it's deps as inputs.
    # On success create the file deps_all_done_first.txt.
    {
      'target_name': 'action_with_dependencies_123',
      'type': 'none',
      'dependencies': [ 'dep_1', 'dep_2', 'dep_3' ],
      'actions': [{
        'inputs': [ 'actions.gyp' ],
        'outputs': [ 'deps_all_done_first_123.txt' ],
        'action_name': 'action_with_dependencies_123',
        'action': [ 'python', 'confirm-dep-files.py', '<(_outputs)' ],
        # Allows the test to run without hermetic cygwin on windows.
        'msvs_cygwin_shell': 0,
      }],
    },
    # Same as above but with deps in reverse.
    {
      'target_name': 'action_with_dependencies_321',
      'type': 'none',
      'dependencies': [ 'dep_3', 'dep_2', 'dep_1' ],
      'actions': [{
        'inputs': [ 'actions.gyp' ],
        'outputs': [ 'deps_all_done_first_321.txt' ],
        'action_name': 'action_with_dependencies_321',
        'action': [ 'python', 'confirm-dep-files.py', '<(_outputs)' ],
        # Allows the test to run without hermetic cygwin on windows.
        'msvs_cygwin_shell': 0,
      }],
    },

  ],
}
