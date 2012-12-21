# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is included from commands.gyp to test evaluation order of includes.
{
  'variables': {
    'included_variable': 'XYZ',
  },
  'targets': [
    {
      'target_name': 'dummy',
      'type': 'none',
    },
  ],
}
