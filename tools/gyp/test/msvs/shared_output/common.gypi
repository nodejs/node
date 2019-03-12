# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'default_configuration': 'Baz',
    'configurations': {
      'Baz': {
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)/foo',
          'IntermediateDirectory': '$(OutDir)/bar',
        },
      },
    },
  },
}
