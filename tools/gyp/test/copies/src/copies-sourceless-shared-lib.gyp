# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mylib',
      'type': 'static_library',
      'sources': [ 'foo.c' ],
    },
    {
      'target_name': 'mysolib',
      'type': 'shared_library',
      'dependencies': [ 'mylib' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/copies-out',
          'files': [ 'file1' ],
        },
      ],
      # link.exe gets confused by sourceless shared libraries and needs this
      # to become unconfused.
      'msvs_settings': { 'VCLinkerTool': { 'TargetMachine': '1', }, },
    },
  ],
}
