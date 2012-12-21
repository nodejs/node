# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'configurations',
      'type': 'executable',
      'sources': [
        'configurations.c',
      ],
      'configurations': {
        'Debug': {
          'defines': [
            'DEBUG',
          ],
        },
        'Release': {
          'defines': [
            'RELEASE',
          ],
        },
        'Foo': {
          'defines': [
            'FOO',
          ],
        },
      }
    },
  ],
}
