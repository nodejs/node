# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'top',
      'type': 'none',
      'dependencies': [ 'file_cycle1.gyp:middle' ],
    },
    {
      'target_name': 'bottom',
      'type': 'none',
    },
  ],
}
