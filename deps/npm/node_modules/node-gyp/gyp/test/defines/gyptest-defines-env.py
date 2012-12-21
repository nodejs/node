#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable with C++ define specified by a gyp define.
"""

import os
import TestGyp

test = TestGyp.TestGyp()

# With the value only given in environment, it should be used.
try:
  os.environ['GYP_DEFINES'] = 'value=10'
  test.run_gyp('defines-env.gyp')
finally:
  del os.environ['GYP_DEFINES']

test.build('defines-env.gyp')

expect = """\
VALUE is 10
"""
test.run_built_executable('defines', stdout=expect)


# With the value given in both command line and environment,
# command line should take precedence.
try:
  os.environ['GYP_DEFINES'] = 'value=20'
  test.run_gyp('defines-env.gyp', '-Dvalue=25')
finally:
  del os.environ['GYP_DEFINES']

test.sleep()
test.touch('defines.c')
test.build('defines-env.gyp')

expect = """\
VALUE is 25
"""
test.run_built_executable('defines', stdout=expect)


# With the value only given in environment, it should be ignored if
# --ignore-environment is specified.
try:
  os.environ['GYP_DEFINES'] = 'value=30'
  test.run_gyp('defines-env.gyp', '--ignore-environment')
finally:
  del os.environ['GYP_DEFINES']

test.sleep()
test.touch('defines.c')
test.build('defines-env.gyp')

expect = """\
VALUE is 5
"""
test.run_built_executable('defines', stdout=expect)


# With the value given in both command line and environment, and
# --ignore-environment also specified, command line should still be used.
try:
  os.environ['GYP_DEFINES'] = 'value=40'
  test.run_gyp('defines-env.gyp', '--ignore-environment', '-Dvalue=45')
finally:
  del os.environ['GYP_DEFINES']

test.sleep()
test.touch('defines.c')
test.build('defines-env.gyp')

expect = """\
VALUE is 45
"""
test.run_built_executable('defines', stdout=expect)


test.pass_test()
