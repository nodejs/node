# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'foo',
      'type': 'executable',
      'rules': [
        {
          'rule_name': 'bar',
          'extension': '',
        },
        {
          'rule_name': 'bar',
          'extension': '',
        },
      ],
    },
  ],
}
