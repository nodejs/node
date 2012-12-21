# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
  {
   'target_name': 'lib',
   'product_name': 'Test',
   'type': 'static_library',
   'sources': [ 'my_file.cc' ],
  },
  {
   'target_name': 'exe',
   'product_name': 'Test',
   'type': 'executable',
   'dependencies': [ 'lib' ],
   'sources': [ 'my_main_file.cc' ],
  },
 ]
}
