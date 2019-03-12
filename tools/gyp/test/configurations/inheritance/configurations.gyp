# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'configurations': {
      'Base': {
         'abstract': 1,
         'defines': ['BASE'],
      },
      'Common': {
         'abstract': 1,
         'inherit_from': ['Base'],
         'defines': ['COMMON'],
      },
      'Common2': {
         'abstract': 1,
         'defines': ['COMMON2'],
      },
      'Debug': {
        'inherit_from': ['Common', 'Common2'],
        'defines': ['DEBUG'],
      },
      'Release': {
        'inherit_from': ['Common', 'Common2'],
        'defines': ['RELEASE'],
      },
    },
  },
  'targets': [
    {
      'target_name': 'configurations',
      'type': 'executable',
      'sources': [
        'configurations.c',
      ],
    },
  ],
}
