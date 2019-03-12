# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hello',
      'type': 'executable',
      'sources': [
        'hello.cpp',
        'hello_excluded.cpp',
      ],
      'sources!': [
        'hello_excluded.cpp',
      ],
    },
  ],
}
