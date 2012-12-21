# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'set_symroot%': 0,
  },
  'conditions': [
    ['set_symroot == 1', {
      'xcode_settings': {
        'SYMROOT': '<(DEPTH)/build',
      },
    }],
  ],
}
