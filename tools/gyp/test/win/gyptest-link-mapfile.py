#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure mapfile settings are extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('mapfile.gyp', chdir=CHDIR)
  test.build('mapfile.gyp', test.ALL, chdir=CHDIR)

  map_file = test.built_file_path('test_mapfile_unset.map', chdir=CHDIR)
  test.must_not_exist(map_file)

  map_file = test.built_file_path('test_mapfile_generate.map', chdir=CHDIR)
  test.must_exist(map_file)
  test.must_contain(map_file, '?AnExportedFunction@@YAXXZ')
  test.must_not_contain(map_file, 'void __cdecl AnExportedFunction(void)')

  map_file = test.built_file_path('test_mapfile_generate_exports.map',
          chdir=CHDIR)
  test.must_exist(map_file)
  test.must_contain(map_file, 'void __cdecl AnExportedFunction(void)')

  map_file = test.built_file_path('test_mapfile_generate_filename.map',
          chdir=CHDIR)
  test.must_not_exist(map_file)

  map_file = test.built_file_path('custom_file_name.map', chdir=CHDIR)
  test.must_exist(map_file)
  test.must_contain(map_file, '?AnExportedFunction@@YAXXZ')
  test.must_not_contain(map_file, 'void __cdecl AnExportedFunction(void)')

  test.pass_test()
