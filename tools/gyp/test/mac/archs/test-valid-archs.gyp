# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
  {
    'target_name': 'lib',
    'product_name': 'Test',
    'type': 'static_library',
    'sources': [ 'my_file.cc' ],
    'xcode_settings': {
      'ARCHS': ['i386', 'x86_64', 'unknown-arch'],
      'VALID_ARCHS': ['x86_64'],
    },
  },
  {
    'target_name': 'exe',
    'product_name': 'Test',
    'type': 'executable',
    'dependencies': [ 'lib' ],
    'sources': [ 'my_main_file.cc' ],
    'xcode_settings': {
      'ARCHS': ['i386', 'x86_64', 'unknown-arch'],
      'VALID_ARCHS': ['x86_64'],
    },
  }]
}
