# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_loadable_module',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'sources': [ 'module.c' ],
      'product_extension': 'plugin',
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
      },
    },
  ],
}
