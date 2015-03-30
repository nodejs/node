# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'copies_samedir',
      'type': 'none',
      'dependencies': [
        'copies_samedir_dependency',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/copies-out-samedir',
          'files': [
            'file1',
          ],
        },
      ],
    },
    {
      'target_name': 'copies_samedir_dependency',
      'type': 'none',
      'direct_dependent_settings': {
        'copies': [
          {
            'destination': '<(PRODUCT_DIR)/copies-out-samedir',
            'files': [
              'file2',
            ],
          },
        ],
      },
    },
  ],
}
