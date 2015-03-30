# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hello1',
      'product_extension': 'stuff',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello2',
      'target_extension': 'stuff',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
    }
  ]
}
