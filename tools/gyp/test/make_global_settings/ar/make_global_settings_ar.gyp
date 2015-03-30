# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style licence that can be
# found in the LICENSE file.

{
  'variables': {
    'custom_ar_target%': '',
    'custom_ar_host%': '',
  },
  'conditions': [
    ['"<(custom_ar_target)"!=""', {
      'make_global_settings': [
        ['AR', '<(custom_ar_target)'],
      ],
    }],
    ['"<(custom_ar_host)"!=""', {
      'make_global_settings': [
        ['AR.host', '<(custom_ar_host)'],
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'make_global_settings_ar_test',
      'type': 'static_library',
      'sources': [ 'foo.c' ],
    },
  ],
}
