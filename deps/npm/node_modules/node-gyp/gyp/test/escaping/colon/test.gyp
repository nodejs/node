# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'colon',
      'type': 'executable',
      'sources': [
        'a:b.c',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/',
          # MSVS2008 gets confused if the same file is in 'sources' and 'copies'
          'files': [ 'a:b.c-d', ],
        },
      ],
    },
  ],
}
