# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'broken_actions2',
      'type': 'none',
      'actions': [
        {
          'inputs': [
            'no_name.input',
          ],
          'action': [
            'python',
            '-c',
            'print \'missing name\'',
          ],
        },
      ],
    },
  ],
}
