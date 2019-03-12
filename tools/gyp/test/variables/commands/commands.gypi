# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is included from commands.gyp to test evaluation order of includes.
{
  'variables': {
    'included_variable': 'XYZ',

    'default_str%': 'my_str',
    'default_empty_str%': '',
    'default_int%': 0,

    'default_empty_files%': '',
    'default_int_files%': 0,
  },
  'targets': [
    {
      'target_name': 'dummy',
      'type': 'none',
    },
  ],
}
