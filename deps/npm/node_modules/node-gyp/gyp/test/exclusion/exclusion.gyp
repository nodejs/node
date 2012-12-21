# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hello',
      'type': 'executable',
      'sources': [
        'hello.c',
        'bogus.c',
        'also/not/real.c',
        'also/not/real2.c',
      ],
      'sources!': [
        'bogus.c',
        'also/not/real.c',
        'also/not/real2.c',
      ],
    },
  ],
}
