# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'actions-test',
      'type': 'none',
      'actions': [
        {
          'action_name': 'first action (fails)',
          'inputs': [
            'action_fail.py',
          ],
          'outputs': [
            'ALWAYS_OUT_OF_DATE',
          ],
          'action': [
            'python', '<@(_inputs)'
          ],
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'second action (succeeds)',
          'inputs': [
            'action_succeed.py',
          ],
          'outputs': [
            'ALWAYS_OUT_OF_DATE',
          ],
          'action': [
            'python', '<@(_inputs)'
          ],
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
