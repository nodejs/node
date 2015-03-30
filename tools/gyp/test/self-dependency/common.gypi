# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A common file that other .gyp files include.
# Makes every target in the project depend on dep.gyp:dep.
{
  'target_defaults': {
    'dependencies': [
      'dep.gyp:dep',
    ],
  },
}
