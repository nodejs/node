# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'nonbundle',
      'type': 'static_library',
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'Postbuild Fail',
          'action': [ './postbuild-fail.sh', ],
        },
        {
          'postbuild_name': 'Runs after failing postbuild',
          'action': [ './touch-static.sh', ],
        },
      ],
    },
    {
      'target_name': 'bundle',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'Postbuild Fail',
          'action': [ './postbuild-fail.sh', ],
        },
        {
          'postbuild_name': 'Runs after failing postbuild',
          'action': [ './touch-dynamic.sh', ],
        },
      ],
    },
  ],
}
