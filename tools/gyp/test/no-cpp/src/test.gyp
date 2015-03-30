# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'no_cpp',
      'type': 'executable',
      'sources': [ 'empty-main.c' ],
    },
    # A static_library with a cpp file and a linkable with only .c files
    # depending on it causes a linker error:
    {
      'target_name': 'cpp_lib',
      'type': 'static_library',
      'sources': [ 'f.cc' ],
    },
    {
      'target_name': 'no_cpp_dep_on_cc_lib',
      'type': 'executable',
      'dependencies': [ 'cpp_lib' ],
      'sources': [ 'call-f-main.c' ],
    },
  ],
}
