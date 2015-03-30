# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'target',
      'product_name': 'Product',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [
        '<(PRODUCT_DIR)/copy.c',
      ],
      'actions': [
        {
          'action_name': 'Helper',
          'description': 'Helps',
          'inputs': [ 'source.c' ],
          'outputs': [ '<(PRODUCT_DIR)/copy.c' ],
          'action': [ 'cp', '${SOURCE_ROOT}/source.c',
                      '<(PRODUCT_DIR)/copy.c' ],
        },
      ],
    },
  ],
}
