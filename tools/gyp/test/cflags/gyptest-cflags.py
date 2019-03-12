#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies the use of the environment during regeneration when the gyp file
changes, specifically via build of an executable with C preprocessor
definition specified by CFLAGS.

In this test, gyp and build both run in same local environment.
"""

import TestGyp

# CPPFLAGS works in ninja but not make; CFLAGS works in both
FORMATS = ('make', 'ninja')

test = TestGyp.TestGyp(formats=FORMATS)

# First set CFLAGS to blank in case the platform doesn't support unsetenv.
with TestGyp.LocalEnv({'CFLAGS': '',
                       'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('cflags.gyp')
  test.build('cflags.gyp')

expect = """FOO not defined\n"""
test.run_built_executable('cflags', stdout=expect)
test.run_built_executable('cflags_host', stdout=expect)

test.sleep()

with TestGyp.LocalEnv({'CFLAGS': '-DFOO=1',
                       'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('cflags.gyp')
  test.build('cflags.gyp')

expect = """FOO defined\n"""
test.run_built_executable('cflags', stdout=expect)

# Environment variable CFLAGS shouldn't influence the flags for the host.
expect = """FOO not defined\n"""
test.run_built_executable('cflags_host', stdout=expect)

test.sleep()

with TestGyp.LocalEnv({'CFLAGS_host': '-DFOO=1',
                       'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('cflags.gyp')
  test.build('cflags.gyp')

# Environment variable CFLAGS_host should influence the flags for the host.
expect = """FOO defined\n"""
test.run_built_executable('cflags_host', stdout=expect)

test.sleep()

with TestGyp.LocalEnv({'CFLAGS': ''}):
  test.run_gyp('cflags.gyp')
  test.build('cflags.gyp')

expect = """FOO not defined\n"""
test.run_built_executable('cflags', stdout=expect)

test.sleep()

with TestGyp.LocalEnv({'CFLAGS': '-DFOO=1'}):
  test.run_gyp('cflags.gyp')
  test.build('cflags.gyp')

expect = """FOO defined\n"""
test.run_built_executable('cflags', stdout=expect)

test.pass_test()
