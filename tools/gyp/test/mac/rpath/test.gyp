# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'default_rpath',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
    },
    {
      'target_name': 'explicit_rpath',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
      'xcode_settings': {
        'LD_RUNPATH_SEARCH_PATHS': ['@executable_path/.'],
      },
    },
    {
      'target_name': 'explicit_rpaths_escaped',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
      'xcode_settings': {
        # Xcode requires spaces to be escaped, else it ends up adding two
        # independent rpaths.
        'LD_RUNPATH_SEARCH_PATHS': ['First\\ rpath', 'Second\\ rpath'],
      },
    },
    {
      'target_name': 'explicit_rpaths_bundle',
      'product_name': 'My Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c' ],
      'xcode_settings': {
        'LD_RUNPATH_SEARCH_PATHS': ['@loader_path/.'],
      },
    },
    {
      'target_name': 'executable',
      'type': 'executable',
      'sources': [ 'main.c' ],
      'xcode_settings': {
        'LD_RUNPATH_SEARCH_PATHS': ['@executable_path/.'],
      },
    },
  ],
}
