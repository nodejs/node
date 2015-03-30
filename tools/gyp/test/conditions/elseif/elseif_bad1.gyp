# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Trigger an error because of two consecutive string conditions.

{
  'targets': [
    {
      'variables': { 'test_var': 0 },
      'target_name': 'program',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'conditions': [
        ['test_var==0', 'test_var==1', {
        }],
      ],
    },
  ],
}
