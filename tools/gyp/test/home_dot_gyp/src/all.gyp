# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'foo%': '"fromdefault"',
  },
  'targets': [
    {
      'target_name': 'printfoo',
      'type': 'executable',
      'sources': [
        'printfoo.c',
      ],
      'defines': [
        'FOO=<(foo)',
      ],
    },
  ],
}

