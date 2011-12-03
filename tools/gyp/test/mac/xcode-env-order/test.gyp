# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'test_app',
      'product_name': 'Test',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'main.c',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
        'STRING_KEY': '/Source/Project',
        'DEPENDENT_KEY2': '$(STRING_KEY)/$(PRODUCT_NAME)',
        'DEPENDENT_KEY1': 'DEP:$(DEPENDENT_KEY2)',
        'DEPENDENT_KEY3': '$(PRODUCT_TYPE):$(DEPENDENT_KEY1)',
      },
    },
  ],
}
