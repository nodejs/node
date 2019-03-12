# Copyright (c) 2017 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ldflags',
      'type': 'executable',
      'sources': [
        'main.c',
      ],
    },
    {
      'target_name': 'ldflags_host',
      'toolsets': ['host'],
      'type': 'executable',
      'sources': [
        'main.c',
      ],
    },
  ],
}
