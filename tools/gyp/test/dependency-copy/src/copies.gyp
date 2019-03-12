# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'proj1',
      'type': 'executable',
      'sources': [
        'file1.c',
      ],
    },
    {
      'target_name': 'proj2',
      'type': 'executable',
      'sources': [
        'file2.c',
      ],
      'dependencies': [
        'proj1',
      ]
    },
  ],
}
