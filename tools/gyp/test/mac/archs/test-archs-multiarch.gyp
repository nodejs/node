# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'static_32_64',
      'type': 'static_library',
      'sources': [ 'my_file.cc' ],
      'xcode_settings': {
        'ARCHS': [ 'i386', 'x86_64' ],
      },
    },
    {
      'target_name': 'shared_32_64',
      'type': 'shared_library',
      'sources': [ 'my_file.cc' ],
      'xcode_settings': {
        'ARCHS': [ 'i386', 'x86_64' ],
      },
    },
    {
      'target_name': 'shared_32_64_bundle',
      'product_name': 'My Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'my_file.cc' ],
      'xcode_settings': {
        'ARCHS': [ 'i386', 'x86_64' ],
      },
    },
    {
      'target_name': 'module_32_64',
      'type': 'loadable_module',
      'sources': [ 'my_file.cc' ],
      'xcode_settings': {
        'ARCHS': [ 'i386', 'x86_64' ],
      },
    },
    {
      'target_name': 'module_32_64_bundle',
      'product_name': 'My Bundle',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'sources': [ 'my_file.cc' ],
      'xcode_settings': {
        'ARCHS': [ 'i386', 'x86_64' ],
      },
    },
    {
      'target_name': 'exe_32_64',
      'type': 'executable',
      'sources': [ 'empty_main.cc' ],
      'xcode_settings': {
        'ARCHS': [ 'i386', 'x86_64' ],
      },
    },
    {
      'target_name': 'exe_32_64_bundle',
      'product_name': 'Test App',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'empty_main.cc' ],
      'xcode_settings': {
        'ARCHS': [ 'i386', 'x86_64' ],
      },
    },
    # This only needs to compile.
    {
      'target_name': 'precompiled_prefix_header_mm_32_64',
      'type': 'shared_library',
      'sources': [ 'file.mm', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
        'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
      },
    },
    # This does not compile but should not cause generation errors.
    {
      'target_name': 'exe_32_64_no_sources',
      'type': 'executable',
      'dependencies': [
        'static_32_64',
      ],
      'sources': [],
      'xcode_settings': {
        'ARCHS': ['i386', 'x86_64'],
      },
    },
  ]
}
