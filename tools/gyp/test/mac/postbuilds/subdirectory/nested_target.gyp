# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'nest_el',
      'type': 'static_library',
      'sources': [ '../file_g.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'Static library postbuild',
          'variables': {
            'some_regex': 'a|b',
          },
          'action': [
            '../script/static_library_postbuild.sh',
            '<(some_regex)',
            'arg with spaces',
          ],
        },
      ],
    },
    {
      'target_name': 'nest_dyna',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ '../file_h.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'Dynamic library postbuild',
          'variables': {
            'some_regex': 'a|b',
          },
          'action': [
            '../script/shared_library_postbuild.sh',
            '<(some_regex)',
            'arg with spaces',
          ],
        },
        {
          'postbuild_name': 'Test paths relative to gyp file',
          'action': [
            '../copy.sh',
            './copied_file.txt',
            '${BUILT_PRODUCTS_DIR}/copied_file_2.txt',
          ],
        },
      ],
    },
  ],
}

