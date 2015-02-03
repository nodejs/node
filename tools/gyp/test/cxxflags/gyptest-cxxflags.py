#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies the use of the environment during regeneration when the gyp file
changes, specifically via build of an executable with C++ flags specified by
CXXFLAGS.

In this test, gyp happens within a local environment, but build outside of it.
"""

import TestGyp

FORMATS = ('ninja',)

test = TestGyp.TestGyp(formats=FORMATS)

# We reset the environ after calling gyp. When the auto-regeneration happens,
# the same define should be reused anyway.
with TestGyp.LocalEnv({'CXXFLAGS': ''}):
  test.run_gyp('cxxflags.gyp')

test.build('cxxflags.gyp')

expect = """\
No define
"""
test.run_built_executable('cxxflags', stdout=expect)

test.sleep()

with TestGyp.LocalEnv({'CXXFLAGS': '-DABC'}):
  test.run_gyp('cxxflags.gyp')

test.build('cxxflags.gyp')

expect = """\
With define
"""
test.run_built_executable('cxxflags', stdout=expect)

test.pass_test()
