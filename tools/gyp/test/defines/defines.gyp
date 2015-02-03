# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'defines',
      'type': 'executable',
      'sources': [
        'defines.c',
      ],
      'defines': [
        'FOO',
        'VALUE=1',
        'PAREN_VALUE=(1+2+3)',
        'HASH_VALUE="a#1"',
      ],
    },
  ],
  'conditions': [
    ['OS=="fakeos"', {
      'targets': [
        {
          'target_name': 'fakeosprogram',
          'type': 'executable',
          'sources': [
            'defines.c',
          ],
          'defines': [
            'FOO',
            'VALUE=1',
          ],
        },
      ],
    }],
  ],
}
