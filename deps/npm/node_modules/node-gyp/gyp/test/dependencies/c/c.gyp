# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'c_unused',
      'type': 'static_library',
      'sources': [
        'c.c',
      ],
    },
    {
      'target_name': 'd',
      'type': 'static_library',
      'sources': [
        'd.c',
      ],
    },
  ],
}
