# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'test_bundle',
      'product_name': 'My Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'empty.c', ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Framework-Info.plist',
      },
      'mac_bundle_resources': [
        'resource_file.sb',
      ],
    },
    {
      'target_name': 'test_app',
      'product_name': 'Test App',
      'type': 'executable',
      'mac_bundle': 1,
      'dependencies': [
        'test_bundle',
      ],
      'sources': [ 'main.c', ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'TestApp-Info.plist',
      },
      'postbuilds': [
        {
          'postbuild_name': 'Copy dependent framework into app',
          'action': [
            './postbuild-copy-framework.sh',
            '${BUILT_PRODUCTS_DIR}/My Framework.framework',
            '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/',
          ],
        },
      ],
    },
  ],
}
