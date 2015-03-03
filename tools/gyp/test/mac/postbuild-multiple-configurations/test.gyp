# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'configurations': {
       'Debug': {},
       'Release': {},
    },
  },
  'targets': [
    {
      'target_name': 'random_target',
      'type': 'executable',
      'sources': [ 'main.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'Touch a file.',
          'action': [
            './postbuild-touch-file.sh',
          ],
        },
      ],
    },
  ],
}
