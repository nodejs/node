# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'test_app',
      'product_name': 'Test App',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'main.c',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'TestApp-Info.plist',
      },
    },
    {
      'target_name': 'test_app_postbuilds',
      'product_name': 'Test App 2',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'main.c',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'TestApp-Info.plist',
      },
      'postbuilds': [
        {
          'postbuild_name': 'Postbuild that touches the app binary',
          'action': [
            './delay-touch.sh', '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
          ],
        },
      ],
    },
    {
      'target_name': 'test_framework_postbuilds',
      'product_name': 'Test Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [
        'empty.c',
      ],
      'postbuilds': [
        {
          'postbuild_name': 'Postbuild that touches the framework binary',
          'action': [
            './delay-touch.sh', '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
          ],
        },
      ],
    },
  ],
}
