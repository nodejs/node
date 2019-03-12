#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test that RelativePath(s, d) doesn't return a path starting with '..' when
s is textually below d, but is also a symlink to a file that is not below d.

Returning .. in this case would break the Ninja generator in such a case,
because it computes output directories by concatenating paths, and concat'ing
a path starting with .. can unexpectedly erase other parts of the path. It's
difficult to test this directly since the test harness assumes toplevel_dir is
the root of the repository, but this test should at least verify that the
required behavior doesn't change.
"""

import TestGyp
import os
import sys
import tempfile

if sys.platform != 'win32':
  test = TestGyp.TestGyp()

  # Copy hello.gyp and hello.c to temporary named files, which will then be
  # symlinked back and processed. Note that we don't ask gyp to touch the
  # original files at all; they are only there as source material for the copy.
  # That's why hello.gyp references symlink_hello.c instead of hello.c.
  with tempfile.NamedTemporaryFile(mode='w+') as gyp_file:
    with tempfile.NamedTemporaryFile(mode='w+') as c_file:
      with open('hello.gyp') as orig_gyp_file:
        gyp_file.write(orig_gyp_file.read())
        gyp_file.flush()
      with open('hello.c') as orig_c_file:
        c_file.write(orig_c_file.read())
        c_file.flush()
      # We need to flush the files because we want to read them before closing
      # them, since when they are closed they will be deleted.

      # Don't proceed with the test on a system that doesn't let you read from
      # a still-open temporary file.
      if os.path.getsize(gyp_file.name) == 0:
        raise OSError("Copy to temporary file didn't work.")

      symlink_gyp = test.built_file_path('symlink_hello.gyp')
      symlink_c = test.built_file_path('symlink_hello.c')
      outdir = os.path.dirname(symlink_gyp)

      # Make sure the outdir exists.
      try:
        os.makedirs(outdir)
      except OSError:
        if not os.path.isdir(outdir):
          raise
      os.symlink(gyp_file.name, symlink_gyp)
      os.symlink(c_file.name, symlink_c)

      # Run gyp on the symlinked files.
      test.run_gyp(symlink_gyp, chdir=outdir)
      test.build(symlink_gyp, chdir=outdir)
      test.run_built_executable('symlink_hello', stdout="Hello, world!\n",
                                chdir=outdir)

      test.pass_test()
