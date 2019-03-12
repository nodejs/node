#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure libpath is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'

  # Build subdirectory library.
  test.run_gyp('subdir/library.gyp', chdir=CHDIR)
  test.build('subdir/library.gyp', test.ALL, chdir=CHDIR)

  # And then try to link the main project against the library using only
  # LIBPATH to find it.
  test.run_gyp('library-directories.gyp', chdir=CHDIR)

  # Without additional paths specified, should fail.
  test.build('library-directories.gyp', 'test_libdirs_none', chdir=CHDIR,
      status=1)

  # With the additional library directory, should pass.
  test.build('library-directories.gyp', 'test_libdirs_with', chdir=CHDIR)

  test.pass_test()
