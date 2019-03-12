# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'ldflags_passed_to_libtool',
      'type': 'static_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'OTHER_LDFLAGS': [
          '-fblorfen-horf-does-not-exist',
        ],
      },
    },
  ],
}
