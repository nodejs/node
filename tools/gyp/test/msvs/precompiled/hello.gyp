# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hello',
      'type': 'executable',
      'sources': [
        'hello.c',
        'hello2.c',
        'precomp.c',
      ],
      'msvs_precompiled_header': 'stdio.h',
      'msvs_precompiled_source': 'precomp.c',
    },
  ],
}
