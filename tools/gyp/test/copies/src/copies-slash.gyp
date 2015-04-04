# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # A trailing slash on the destination directory should be ignored.
    {
      'target_name': 'copies_recursive_trailing_slash',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/copies-out-slash/',
          'files': [
            'directory/',
          ],
        },
      ],
    },
    # Even if the source directory is below <(PRODUCT_DIR).
    {
      'target_name': 'copies_recursive_trailing_slash_in_product_dir',
      'type': 'none',
      'dependencies': [ ':copies_recursive_trailing_slash' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/copies-out-slash-2/',
          'files': [
            '<(PRODUCT_DIR)/copies-out-slash/directory/',
          ],
        },
      ],
    },
  ],
}

