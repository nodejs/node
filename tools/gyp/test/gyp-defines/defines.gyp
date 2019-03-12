# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_target',
      'type': 'none',
      'actions': [
        {
          'action_name': 'test_action',
          'inputs': [],
          'outputs': [ 'action.txt' ],
          'action': [
            'python',
            'echo.py',
            '<(key)',
            '<(_outputs)',
          ],
          'msvs_cygwin_shell': 0,
        }
      ],
    },
  ],
}
