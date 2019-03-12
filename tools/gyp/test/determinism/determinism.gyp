# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'determinism',
      'type': 'none',
      'actions': [
        {
          'action_name': 'depfile_action',
          'inputs': [
            'input.txt',
          ],
          'outputs': [
            'output.txt',
          ],
          'depfile': 'depfile.d',
          'action': [ ]
        },
      ],
    },
    {
      'target_name': 'determinism2',
      'type': 'none',
      'actions': [
        {
          'action_name': 'depfile_action',
          'inputs': [
            'input.txt',
          ],
          'outputs': [
            'output.txt',
          ],
          'depfile': 'depfile.d',
          'action': [ ]
        },
      ],
    },
    {
      'target_name': 'determinism3',
      'type': 'none',
      'actions': [
        {
          'action_name': 'depfile_action',
          'inputs': [
            'input.txt',
          ],
          'outputs': [
            'output.txt',
          ],
          'depfile': 'depfile.d',
          'action': [ ]
        },
      ],
    },
  ],
}
