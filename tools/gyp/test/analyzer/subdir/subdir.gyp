# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'trailing_dir_path': '../',
   },
  'targets': [
    {
      'target_name': 'foo',
      'type': 'static_library',
      'sources': [
        'subdir_source.c',
        '<(trailing_dir_path)/parent_source.c',
      ],
    },
    {
      'target_name': 'subdir2a',
      'type': 'static_library',
      'sources': [
        'subdir2_source.c',
      ],
      'dependencies': [
        'subdir2b',
      ],
    },
    {
      'target_name': 'subdir2b',
      'type': 'static_library',
      'sources': [
        'subdir2b_source.c',
      ],
    },
  ],
}
