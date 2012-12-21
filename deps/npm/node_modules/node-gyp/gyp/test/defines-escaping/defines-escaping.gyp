# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'defines_escaping',
      'type': 'executable',
      'sources': [
        'defines-escaping.c',
      ],
      'defines': [
        'TEST_FORMAT="<(test_format)"',
        'TEST_ARGS=<(test_args)',
      ],
    },
  ],
}
