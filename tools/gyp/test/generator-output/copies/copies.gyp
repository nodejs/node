# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'pull_in_subdir',
      'type': 'none',
      'dependencies': [
        'subdir/subdir.gyp:*',
      ],
    },
    {
      'target_name': 'copies1',
      'type': 'none',
      'copies': [
        {
          'destination': 'copies-out',
          'files': [
            'file1',
          ],
        },
      ],
    },
    {
      'target_name': 'copies2',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/copies-out',
          'files': [
            'file2',
          ],
        },
      ],
    },
    # Verify that a null 'files' list doesn't gag the generators.
    {
      'target_name': 'copies_null',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/copies-null',
          'files': [],
        },
      ],
    },
  ],
}
