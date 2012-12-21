# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'tools',
      'type': 'executable',
      'sources': [
        'tools.c',
      ],
    },
  ],
  'scons_settings': {
    'tools': ['default', 'this_tool'],
  },
}
