# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
  {
   'target_name': 'lib',
   'product_name': 'Test64',
   'type': 'static_library',
   'sources': [ 'my_file.cc' ],
   'xcode_settings': {
     'ARCHS': [ 'x86_64' ],
   },
  },
  {
   'target_name': 'exe',
   'product_name': 'Test64',
   'type': 'executable',
   'dependencies': [ 'lib' ],
   'sources': [ 'my_main_file.cc' ],
   'xcode_settings': {
     'ARCHS': [ 'x86_64' ],
   },
  },
 ]
}
