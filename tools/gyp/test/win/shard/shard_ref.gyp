# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'refs_to_shard_external_lib',
      'type': 'static_library',
      'dependencies': [
        # Make sure references in other files are updated correctly.
        'shard.gyp:shard',
      ],
      'sources': [
        'hello.cc',
      ],
    },
    {
      'target_name': 'refs_to_shard_external_exe',
      'type': 'executable',
      'dependencies': [
        # Make sure references in other files are updated correctly.
        'shard.gyp:shard',
      ],
      'sources': [
        'hello.cc',
      ],
    },
    {
      'target_name': 'refs_to_shard_external_dll',
      'type': 'shared_library',
      'dependencies': [
        # Make sure references in other files are updated correctly.
        'shard.gyp:shard',
      ],
      'sources': [
        'hello.cc',
      ],
    },
  ]
}
