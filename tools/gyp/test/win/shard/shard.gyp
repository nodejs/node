# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'shard',
      'type': 'static_library',
      'msvs_shard': 4,
      'sources': [
        'hello1.cc',
        'hello2.cc',
        'hello3.cc',
        'hello4.cc',
      ],
      'product_dir': '<(PRODUCT_DIR)',
    },
    {
      'target_name': 'refs_to_shard',
      'type': 'executable',
      'dependencies': [
        # Make sure references are correctly updated.
        'shard',
      ],
      'sources': [
        'hello.cc',
      ],
    },
  ]
}
