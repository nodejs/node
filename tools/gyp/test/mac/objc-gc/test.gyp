# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    # For some reason, static_library targets that are built with gc=required
    # and then linked to executables that don't use gc, the linker doesn't
    # complain. For shared_libraries it does, so use that.
    {
      'target_name': 'no_gc_lib',
      'type': 'shared_library',
      'sources': [
        'c-file.c',
        'cc-file.cc',
        'needs-gc-mm.mm',
        'needs-gc.m',
      ],
    },
    {
      'target_name': 'gc_lib',
      'type': 'shared_library',
      'sources': [
        'c-file.c',
        'cc-file.cc',
        'needs-gc-mm.mm',
        'needs-gc.m',
      ],
      'xcode_settings': {
        'GCC_ENABLE_OBJC_GC': 'supported',
      },
    },
    {
      'target_name': 'gc_req_lib',
      'type': 'shared_library',
      'sources': [
        'c-file.c',
        'cc-file.cc',
        'needs-gc-mm.mm',
        'needs-gc.m',
      ],
      'xcode_settings': {
        'GCC_ENABLE_OBJC_GC': 'required',
      },
    },

    {
      'target_name': 'gc_exe_fails',
      'type': 'executable',
      'sources': [ 'main.m' ],
      'dependencies': [ 'no_gc_lib' ],
      'xcode_settings': {
        'GCC_ENABLE_OBJC_GC': 'required',
      },
      'libraries': [ 'Foundation.framework' ],
    },
    {
      'target_name': 'gc_req_exe',
      'type': 'executable',
      'sources': [ 'main.m' ],
      'dependencies': [ 'gc_lib' ],
      'xcode_settings': {
        'GCC_ENABLE_OBJC_GC': 'required',
      },
      'libraries': [ 'Foundation.framework' ],
    },
    {
      'target_name': 'gc_exe_req_lib',
      'type': 'executable',
      'sources': [ 'main.m' ],
      'dependencies': [ 'gc_req_lib' ],
      'xcode_settings': {
        'GCC_ENABLE_OBJC_GC': 'supported',
      },
      'libraries': [ 'Foundation.framework' ],
    },
    {
      'target_name': 'gc_exe',
      'type': 'executable',
      'sources': [ 'main.m' ],
      'dependencies': [ 'gc_lib' ],
      'xcode_settings': {
        'GCC_ENABLE_OBJC_GC': 'supported',
      },
      'libraries': [ 'Foundation.framework' ],
    },
    {
      'target_name': 'gc_off_exe_req_lib',
      'type': 'executable',
      'sources': [ 'main.m' ],
      'dependencies': [ 'gc_req_lib' ],
      'libraries': [ 'Foundation.framework' ],
    },
    {
      'target_name': 'gc_off_exe',
      'type': 'executable',
      'sources': [ 'main.m' ],
      'dependencies': [ 'gc_lib' ],
      'libraries': [ 'Foundation.framework' ],
    },
  ],
}

