# Copyright 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These gyp files create the following dependencies:
#
# test.gyp:
#   #a -> b
#     a.c
#   #b
#     b.c
#  a and b are static libraries.

{
  'targets': [
    {
      'target_name': 'a',
      'type': 'static_library',
      'sources': [
        'a.c',
      ],
      'dependencies': [
        'b',
      ],
    },
    {
      'target_name': 'b',
      'type': 'static_library',
      'sources': [
        'b.c',
      ],
    },
  ],
}
