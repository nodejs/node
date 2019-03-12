# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hello',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
      'msvs_configuration_attributes': {
        'OutputDirectory':'$(SolutionDir)VC90/'
      },
      'msbuild_configuration_attributes': {
        'OutputDirectory':'$(SolutionDir)VC100/',
      },
    },
  ],
}
