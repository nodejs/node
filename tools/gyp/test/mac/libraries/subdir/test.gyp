# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libraries-test',
      'type': 'executable',
      'sources': [
        'hello.cc',
      ],
      'link_settings': {
        'libraries': [
          'libcrypto.dylib',
          'libfl.a',
        ],
      },
    },
    {
      # This creates a static library and puts it in a nonstandard location for
      # libraries-search-path-test.
      'target_name': 'mylib',
      'type': 'static_library',
      'sources': [
        'mylib.c',
      ],
      'postbuilds': [
        {
          'postbuild_name': 'Make a secret location',
          'action': [
            'mkdir',
            '-p',
            '${SRCROOT}/../secret_location',
          ],
        },
        {
          'postbuild_name': 'Copy to secret location, with secret name',
          'action': [
            'cp',
            '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
            '${SRCROOT}/../secret_location/libmysecretlib.a',
          ],
        },
      ],
    },
    {
      'target_name': 'libraries-search-path-test',
      'type': 'executable',
      'dependencies': [ 'mylib' ],
      'sources': [
        'hello.cc',
      ],
      'xcode_settings': {
        'LIBRARY_SEARCH_PATHS': [
          '<(DEPTH)/secret_location',
        ],
      },
      'link_settings': {
        'libraries': [
          'libmysecretlib.a',
        ],
      },
    },
  ],
}
