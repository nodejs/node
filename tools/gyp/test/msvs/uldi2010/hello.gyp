# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hello',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
      'dependencies': [
        'hellolib',
      ]
    },
    {
      'target_name': 'hellolib',
      'type': 'static_library',
      'sources': [
        'hello2.c',
      ],
      'msvs_2010_disable_uldi_when_referenced': 1,
    },
  ],
}
