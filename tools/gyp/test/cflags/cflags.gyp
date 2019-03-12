# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cflags',
      'type': 'executable',
      'sources': [
        'cflags.c',
      ],
    },
    {
      'target_name': 'cflags_host',
      'toolsets': ['host'],
      'type': 'executable',
      'sources': [
        'cflags.c',
      ],
    },
  ],
}
