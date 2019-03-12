# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'hello',
      'product_name': '$(AppName)',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
      'msvs_props': [
        '$(SolutionDir)AppName.vsprops'
      ],
      'msbuild_props': [
        '$(SolutionDir)AppName.props'
      ],
    },
  ],
}

