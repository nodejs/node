# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'dep_framework',
      'product_name': 'Dependency Bundle',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'empty.c', ],
    },
    {    
      'target_name': 'test_framework',
      'product_name': 'Test Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'dependencies': [ 'dep_framework', ],
      'sources': [
        'TestFramework/ObjCVector.h',
        'TestFramework/ObjCVectorInternal.h',
        'TestFramework/ObjCVector.mm',
      ],
      'mac_framework_headers': [
        'TestFramework/ObjCVector.h',
      ],
      'mac_bundle_resources': [
        'TestFramework/English.lproj/InfoPlist.strings',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
        ],
      },
      'xcode_settings': {
        'INFOPLIST_FILE': 'TestFramework/Info.plist',
        'GCC_DYNAMIC_NO_PIC': 'NO',
      },
      'copies': [
        # Test copying to a file that has envvars in its dest path.
        # Needs to be in a mac_bundle target, else CONTENTS_FOLDER_PATH isn't
        # set.
        {
          'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries',
          'files': [
            'empty.c',
          ],
        },
      ],
    },
    {
      'target_name': 'copy_target',
      'type': 'none',
      'dependencies': [ 'test_framework', 'dep_framework', ],
      'copies': [
        # Test copying directories with spaces in src and dest paths.
        {
          'destination': '<(PRODUCT_DIR)/Test Framework.framework/foo',
          'files': [
            '<(PRODUCT_DIR)/Dependency Bundle.framework',
          ],
        },
      ],
      'actions': [
        {
          'action_name': 'aektschn',
          'inputs': [],
          'outputs': ['<(PRODUCT_DIR)/touched_file'],
          'action': ['touch', '${BUILT_PRODUCTS_DIR}/action_file'],
        },
      ],
    },
  ],
}
