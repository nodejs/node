# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style licence that can be
# found in the LICENSE file.

{
  'variables': {
    'custom_ld_target%': '',
    'custom_ld_host%': '',
  },
  'conditions': [
    ['"<(custom_ld_target)"!=""', {
      'make_global_settings': [
        ['LD', '<(custom_ld_target)'],
      ],
    }],
    ['"<(custom_ld_host)"!=""', {
      'make_global_settings': [
        ['LD.host', '<(custom_ld_host)'],
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'make_global_settings_ld_test',
      'type': 'static_library',
      'sources': [ 'foo.c' ],
    },
  ],
}
