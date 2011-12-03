# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'prefix_header',
      'type': 'static_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
      },
    },
    {
      'target_name': 'precompiled_prefix_header',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'GCC_PREFIX_HEADER': 'header.h',
        'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
      },
    },
  ],
}
