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
      ],
      'actions': [
        {
          'action_name': 'make-prog1',
          'inputs': [
            'make-prog1.py',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/prog1.c',
          ],
          'action': [
            'python', '<(_inputs)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'make-prog2',
          'inputs': [
            'make-prog2.py',
          ],
          'outputs': [
            'actions-out/prog2.c',
          ],
          'action': [
            'python', '<(_inputs)', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
    {
      'target_name': 'counter',
      'type': 'none',
      'actions': [
        {
          # This action should always run, regardless of whether or not it's
          # inputs or the command-line change. We do this by creating a dummy
          # first output, which is always missing, thus causing the build to
          # always try to recreate it. Actual output files should be listed
          # after the dummy one, and dependent targets should list the real
          # output(s) in their inputs
          # (see '../actions.gyp:depend_on_always_run_action').
          'action_name': 'action_counter',
          'inputs': [
            'counter.py',
          ],
          'outputs': [
            'actions-out/action-counter.txt.always',
            'actions-out/action-counter.txt',
          ],
          'action': [
            'python', '<(_inputs)', 'actions-out/action-counter.txt', '2',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
