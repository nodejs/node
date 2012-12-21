# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hello1',
      'type': 'executable',
      'sources': [
        '<(CONFIGURATION_NAME)/hello.cc',
      ],
    },
    {
      'target_name': 'hello2',
      'type': 'executable',
      'sources': [
        './<(CONFIGURATION_NAME)/hello.cc',
      ],
    },
  ],
  'target_defaults': {
    'default_configuration': 'C1',
    'configurations': {
      'C1': {
      },
      'C2': {
      },
    },
  },
}
