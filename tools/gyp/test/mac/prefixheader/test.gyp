# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'prefix_header_c',
      'type': 'static_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
      },
    },
    {
      'target_name': 'precompiled_prefix_header_c',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
        'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
      },
    },

    {
      'target_name': 'prefix_header_cc',
      'type': 'static_library',
      'sources': [ 'file.cc', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
      },
    },
    {
      'target_name': 'precompiled_prefix_header_cc',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.cc', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
        'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
      },
    },

    {
      'target_name': 'prefix_header_m',
      'type': 'static_library',
      'sources': [ 'file.m', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
      },
    },
    {
      'target_name': 'precompiled_prefix_header_m',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.m', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
        'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
      },
    },

    {
      'target_name': 'prefix_header_mm',
      'type': 'static_library',
      'sources': [ 'file.mm', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
      },
    },
    {
      'target_name': 'precompiled_prefix_header_mm',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.mm', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
        'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
      },
    },
  ],
}
