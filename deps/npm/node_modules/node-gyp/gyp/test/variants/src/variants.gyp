# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'variants',
      'type': 'executable',
      'sources': [
        'variants.c',
      ],
      'variants': {
        'variant1' : {
          'defines': [
            'VARIANT1',
          ],
        },
        'variant2' : {
          'defines': [
            'VARIANT2',
          ],
        },
      },
    },
  ],
}
