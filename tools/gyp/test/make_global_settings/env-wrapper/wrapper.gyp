# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'make_global_settings': [
    ['CC', 'clang'],
    ['CC.host', 'clang'],
  ],
  'targets': [
    {
      'target_name': 'test',
      'type': 'static_library',
      'sources': [ 'foo.c' ],
    },
  ],
}
