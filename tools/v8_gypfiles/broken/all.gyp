# Copyright 2011 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'd8.gyp:d8',
        'mkgrokdump.gyp:*',
      ],
      'conditions': [
        ['component!="shared_library"', {
          'dependencies': [
            'parser-shell.gyp:parser-shell',
          ],
        }],
        # These items don't compile for Android on Mac.
        ['host_os!="mac" or OS!="android"', {
          'dependencies': [
            'samples.gyp:*',
          ],
        }],
      ]
    }
  ]
}
