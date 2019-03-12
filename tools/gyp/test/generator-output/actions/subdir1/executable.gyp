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
        },
      ],
    },
  ],
}
