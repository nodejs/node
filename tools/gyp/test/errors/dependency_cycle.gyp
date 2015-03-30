# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'target0',
      'type': 'none',
      'dependencies': [ 'target1' ],
    },
    {
      'target_name': 'target1',
      'type': 'none',
      'dependencies': [ 'target2' ],
    },
    {
      'target_name': 'target2',
      'type': 'none',
      'dependencies': [ 'target0' ],
    },
  ],
}
