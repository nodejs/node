# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'foo',
      'type': '<!(["python", "-c", "import sys; sys.exit(3)"])',
    },
  ]
}
