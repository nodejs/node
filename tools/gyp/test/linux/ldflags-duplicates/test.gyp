# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'make_global_settings': [
    ['LINK_wrapper', './check-ldflags.py'],
  ],
  'targets': [
    {
      'target_name': 'test',
      'type': 'executable',
      'ldflags': [
        '-Wl,--whole-archive <(PRODUCT_DIR)/lib1.a',
        '-Wl,--no-whole-archive',

        '-Wl,--whole-archive <(PRODUCT_DIR)/lib2.a',
        '-Wl,--no-whole-archive',
      ],
      'dependencies': [
        'lib1',
        'lib2',
      ],
      'sources': [
        'main.c',
      ],
    },
    {
      'target_name': 'lib1',
      'type': 'static_library',
      'standalone_static_library': 1,
      'sources': [
        'lib1.c',
      ],
    },
    {
      'target_name': 'lib2',
      'type': 'static_library',
      'standalone_static_library': 1,
      'sources': [
        'lib2.c',
      ],
    },
  ],
}
