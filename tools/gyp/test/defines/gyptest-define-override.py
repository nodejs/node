#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a default gyp define can be overridden.
"""

import os
import TestGyp

test = TestGyp.TestGyp()

# CMake loudly warns about passing '#' to the compiler and drops the define.
expect_stderr = ''
if test.format == 'cmake':
  expect_stderr = (
"""WARNING: Preprocessor definitions containing '#' may not be passed on the"""
""" compiler command line because many compilers do not support it.\n"""
"""CMake is dropping a preprocessor definition: HASH_VALUE="a#1"\n"""
"""Consider defining the macro in a (configured) header file.\n\n""")

# Command-line define
test.run_gyp('defines.gyp', '-D', 'OS=fakeos')
test.build('defines.gyp', stderr=expect_stderr)
test.built_file_must_exist('fakeosprogram', type=test.EXECUTABLE)
# Clean up the exe so subsequent tests don't find an old exe.
os.remove(test.built_file_path('fakeosprogram', type=test.EXECUTABLE))

# Without "OS" override, fokeosprogram shouldn't be built.
test.run_gyp('defines.gyp')
test.build('defines.gyp', stderr=expect_stderr)
test.built_file_must_not_exist('fakeosprogram', type=test.EXECUTABLE)

# Environment define
os.environ['GYP_DEFINES'] = 'OS=fakeos'
test.run_gyp('defines.gyp')
test.build('defines.gyp', stderr=expect_stderr)
test.built_file_must_exist('fakeosprogram', type=test.EXECUTABLE)

test.pass_test()
