# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'el',
      'type': 'static_library',
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'Static library postbuild',
          'variables': {
            'some_regex': 'a|b',
          },
          'action': [
            'script/static_library_postbuild.sh',
            '<(some_regex)',
            'arg with spaces',
          ],
        },
        {
          'postbuild_name': 'Test variable in gyp file',
          'action': [
            'cp',
            '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
            '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}_gyp_touch.a',
          ],
        },
      ],
    },
    {
      'target_name': 'dyna',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'dependencies': [
        'subdirectory/nested_target.gyp:nest_dyna',
        'subdirectory/nested_target.gyp:nest_el',
      ],
      'postbuilds': [
        {
          'postbuild_name': 'Dynamic library postbuild',
          'variables': {
            'some_regex': 'a|b',
          },
          'action': [
            'script/shared_library_postbuild.sh',
            '<(some_regex)',
            'arg with spaces',
          ],
        },
        {
          'postbuild_name': 'Test variable in gyp file',
          'action': [
            'cp',
            '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
            '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}_gyp_touch',
          ],
        },
        {
          'postbuild_name': 'Test paths relative to gyp file',
          'action': [
            './copy.sh',
            'subdirectory/copied_file.txt',
            '${BUILT_PRODUCTS_DIR}',
          ],
        },
      ],
    },
    {
      'target_name': 'dyna_standalone',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'Test variable in gyp file',
          'action': [
            'cp',
            '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
            '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}_gyp_touch.dylib',
          ],
        },
      ],
    },
  ],
}
