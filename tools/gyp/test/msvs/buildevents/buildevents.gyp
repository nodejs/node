# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'main',
      'type': 'executable',
      'sources': [ 'main.cc', ],
      'msvs_prebuild': r'echo starting',
      'msvs_postbuild': r'echo finished',
    },
  ],
}
