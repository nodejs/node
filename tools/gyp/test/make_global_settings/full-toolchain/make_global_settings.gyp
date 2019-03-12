# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style licence that can be
# found in the LICENSE file.

{
  'make_global_settings': [
    ['CC', '/bin/echo MY_CC'],
    ['CXX', '/bin/echo MY_CXX'],
    ['NM', '<(python) <(workdir)/my_nm.py'],
    ['READELF', '<(python) <(workdir)/my_readelf.py'],
  ],
  'targets': [
    {
      'target_name': 'test',
      'type': 'shared_library',
      'sources': [
        'foo.c',
        'bar.cc',
      ],
    },
  ],
}
