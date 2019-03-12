# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'double_dependency',
      'type': 'shared_library',
      'dependencies': [
        'double_dependent.gyp:double_dependent',
      ],
      'conditions': [
        ['1==1', {
          'dependencies': [
            'double_dependent.gyp:*',
          ],
        }],
      ],
    },
  ],
}

