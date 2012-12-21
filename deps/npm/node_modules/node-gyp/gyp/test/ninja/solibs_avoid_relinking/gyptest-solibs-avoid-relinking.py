#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that relinking a solib doesn't relink a dependent executable if the
solib's public API hasn't changed.
"""

import os
import sys
import TestCommon
import TestGyp

# NOTE(fischman): This test will not work with other generators because the
# API-hash-based-mtime-preservation optimization is only implemented in
# ninja.py.  It could be extended to the make.py generator as well pretty
# easily, probably.
# (also, it tests ninja-specific out paths, which would have to be generalized
# if this was extended to other generators).
test = TestGyp.TestGyp(formats=['ninja'])

test.run_gyp('solibs_avoid_relinking.gyp')

# Build the executable, grab its timestamp, touch the solib's source, rebuild
# executable, ensure timestamp hasn't changed.
test.build('solibs_avoid_relinking.gyp', 'b')
test.built_file_must_exist('b' + TestCommon.exe_suffix)
pre_stat = os.stat(test.built_file_path('b' + TestCommon.exe_suffix))
os.utime(os.path.join(test.workdir, 'solib.cc'),
         (pre_stat.st_atime, pre_stat.st_mtime + 100))
test.sleep()
test.build('solibs_avoid_relinking.gyp', 'b')
post_stat = os.stat(test.built_file_path('b' + TestCommon.exe_suffix))

if pre_stat.st_mtime != post_stat.st_mtime:
  test.fail_test()
else:
  test.pass_test()
