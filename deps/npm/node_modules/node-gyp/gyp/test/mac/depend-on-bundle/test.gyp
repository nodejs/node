# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'my_bundle',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'bundle.c' ],
      'mac_bundle_resources': [
        'English.lproj/InfoPlist.strings',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
      }
    },
    {
      'target_name': 'dependent_on_bundle',
      'type': 'executable',
      'sources': [ 'executable.c' ],
      'dependencies': [
        'my_bundle',
      ],
    },
  ],
}

