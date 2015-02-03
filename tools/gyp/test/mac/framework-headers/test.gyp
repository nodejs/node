# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {    
      'target_name': 'test_framework_headers_framework',
      'product_name': 'TestFramework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [
        'myframework.h',
        'myframework.m',
      ],
      'mac_framework_headers': [
        'myframework.h',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        ],
      },
    },{    
      'target_name': 'test_framework_headers_static',
      'product_name': 'TestLibrary',
      'type': 'static_library',
      'xcode_settings': {
        'PUBLIC_HEADERS_FOLDER_PATH': 'include',
      },      
      'sources': [
        'myframework.h',
        'myframework.m',
      ],
      'mac_framework_headers': [
        'myframework.h',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        ],
      },      
    },  
  ],
}
