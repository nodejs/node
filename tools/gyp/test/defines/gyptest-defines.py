#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable with C++ defines.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('defines.gyp')

expect = """\
FOO is defined
VALUE is 1
2*PAREN_VALUE is 12
"""

#CMake loudly warns about passing '#' to the compiler and drops the define.
expect_stderr = ''
if test.format == 'cmake':
  expect_stderr = (
"""WARNING: Preprocessor definitions containing '#' may not be passed on the"""
""" compiler command line because many compilers do not support it.\n"""
"""CMake is dropping a preprocessor definition: HASH_VALUE="a#1"\n"""
"""Consider defining the macro in a (configured) header file.\n\n""")
else:
  expect += """HASH_VALUE is a#1
"""

test.build('defines.gyp', stderr=expect_stderr)

test.run_built_executable('defines', stdout=expect)

test.pass_test()
