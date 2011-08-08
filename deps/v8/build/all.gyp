# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../samples/samples.gyp:*',
        '../src/d8.gyp:d8',
      ],
      'conditions': [
        [ 'component!="shared_library"', {
          'dependencies': [
            '../test/cctest/cctest.gyp:*',
          ],
        }]
      ],
    }
  ]
}

