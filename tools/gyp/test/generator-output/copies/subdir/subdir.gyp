# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'copies3',
      'type': 'none',
      'copies': [
        {
          'destination': 'copies-out',
          'files': [
            'file3',
          ],
        },
      ],
    },
    {
      'target_name': 'copies4',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/copies-out',
          'files': [
            'file4',
          ],
        },
      ],
    },
  ],
}
