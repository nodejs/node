#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of a static_library with the standalone_static_library flag set.
"""

import os
import subprocess
import sys
import TestGyp

test = TestGyp.TestGyp()

# Verify that types other than static_library cause a failure.
test.run_gyp('invalid.gyp', status=1, stderr=None)
target_str = 'invalid.gyp:bad#target'
if test.format == 'scons':
  target_str = os.path.join(os.path.realpath(os.curdir), target_str)
err = ['gyp: Target %s has type executable but standalone_static_library flag '
       'is only valid for static_library type.' % target_str]
test.must_contain_all_lines(test.stderr(), err)

# Build a valid standalone_static_library.
test.run_gyp('mylib.gyp')
test.build('mylib.gyp', target='prog')

# Verify that the static library is copied to the correct location.
if test.format == 'scons':
  # For scons, we expect the library to be copied to the shared lib dir.
  standalone_static_library_dir = test.SHARED_LIB
else:
  # Otherwise, we expect the library to be copied to $PRODUCT_DIR.
  standalone_static_library_dir = test.EXECUTABLE
path_to_lib = os.path.split(
    test.built_file_path('mylib', type=standalone_static_library_dir))[0]
lib_name = test.built_file_basename('mylib', type=test.STATIC_LIB)
path = os.path.join(path_to_lib, lib_name)
test.must_exist(path)

# Verify that the program runs properly.
expect = 'hello from mylib.c\n'
test.run_built_executable('prog', stdout=expect)

# Verify that libmylib.a contains symbols.  "ar -x" fails on a 'thin' archive.
if test.format in ('make', 'ninja') and sys.platform.startswith('linux'):
  retcode = subprocess.call(['ar', '-x', path])
  assert retcode == 0

test.pass_test()