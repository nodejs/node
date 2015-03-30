# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'my_dylib',
      'type': 'shared_library',
      'sources': [ 'empty.c', ],
    },
    {
      'target_name': 'test_app',
      'product_name': 'Test App',
      'type': 'executable',
      'mac_bundle': 1,
      'dependencies': [ 'my_dylib', ],
      'sources': [
        'empty.c',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/Test App.app/Contents/Resources',
          'files': [
            '<(PRODUCT_DIR)/libmy_dylib.dylib',
          ],
        },
      ],
    },
  ],
}

