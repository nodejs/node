# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'no_source_files',
      'type': 'none',
      'sources': [ ],
    },
    {
      'target_name': 'one_source_file',
      'type': 'executable',
      'sources': [
        '../folder/a.c',
      ],
    },
    {
      'target_name': 'two_source_files',
      'type': 'executable',
      'sources': [
        '../folder/a.c',
        '../folder/b.c',
      ],
    },
    {
      'target_name': 'three_files_in_two_folders',
      'type': 'executable',
      'sources': [
        '../folder1/a.c',
        '../folder1/b.c',
        '../folder2/c.c',
      ],
    },
    {
      'target_name': 'nested_folders',
      'type': 'executable',
      'sources': [
        '../folder1/nested/a.c',
        '../folder2/d.c',
        '../folder1/nested/b.c',
        '../folder1/other/c.c',
      ],
    },
  ],
}
