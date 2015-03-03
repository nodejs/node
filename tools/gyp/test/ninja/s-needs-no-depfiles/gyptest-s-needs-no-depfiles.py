#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that .s files don't always trigger a rebuild, as would happen if depfiles
were used for them (since clang & gcc ignore -MMD when building .s->.o on
linux).
"""

import os
import sys
import TestCommon
import TestGyp

# NOTE(fischman): Each generator uses depfiles (or not) differently, so this is
# a ninja-specific test.
test = TestGyp.TestGyp(formats=['ninja'])

if sys.platform == 'win32' or sys.platform == 'win64':
  # This test is about clang/gcc vs. depfiles; VS gets a pass.
  test.pass_test()
  sys.exit(0)

test.run_gyp('s-needs-no-depfiles.gyp')

# Build the library, grab its timestamp, rebuild the library, ensure timestamp
# hasn't changed.
test.build('s-needs-no-depfiles.gyp', 'empty')
empty_dll = test.built_file_path('empty', test.SHARED_LIB)
test.built_file_must_exist(empty_dll)
pre_stat = os.stat(test.built_file_path(empty_dll))
test.sleep()
test.build('s-needs-no-depfiles.gyp', 'empty')
post_stat = os.stat(test.built_file_path(empty_dll))

if pre_stat.st_mtime != post_stat.st_mtime:
  test.fail_test()
else:
  test.pass_test()
