# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'my_lib',
      'type': 'static_library',
      'sources': [ 'empty.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'Postbuild that touches a file',
          'action': [
            './postbuild-touch-file.sh', 'postbuild-file'
          ],
        },
      ],
    },

    {
      'target_name': 'my_sourceless_lib',
      'type': 'static_library',
      'dependencies': [ 'my_lib' ],
      'postbuilds': [
        {
          'postbuild_name': 'Postbuild that touches a file',
          'action': [
            './postbuild-touch-file.sh', 'postbuild-file-sourceless'
          ],
        },
      ],
    },
  ],
}
