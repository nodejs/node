# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'configurations': {
      'Default': {
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\builddir/Default',
        },
      },
    },
  },
  'xcode_settings': {
    'SYMROOT': '<(DEPTH)/builddir',
  },
}
