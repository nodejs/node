#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable with C++ define specified by a gyp define, and
the use of the environment during regeneration when the gyp file changes.
"""

import os
import TestGyp

env_stack = []


def PushEnv():
  env_copy = os.environ.copy()
  env_stack.append(env_copy)

def PopEnv():
  os.eniron=env_stack.pop()

# Regenerating build files when a gyp file changes is currently only supported
# by the make and Android generators.
test = TestGyp.TestGyp(formats=['make', 'android'])

try:
  PushEnv()
  os.environ['CXXFLAGS'] = '-O0'
  test.run_gyp('cxxflags.gyp')
finally:
  # We clear the environ after calling gyp.  When the auto-regeneration happens,
  # the same define should be reused anyway.  Reset to empty string first in
  # case the platform doesn't support unsetenv.
  PopEnv()

test.build('cxxflags.gyp')

expect = """\
Using no optimization flag
"""
test.run_built_executable('cxxflags', stdout=expect)

test.sleep()

try:
  PushEnv()
  os.environ['CXXFLAGS'] = '-O2'
  test.run_gyp('cxxflags.gyp')
finally:
  # We clear the environ after calling gyp.  When the auto-regeneration happens,
  # the same define should be reused anyway.  Reset to empty string first in
  # case the platform doesn't support unsetenv.
  PopEnv()

test.build('cxxflags.gyp')

expect = """\
Using an optimization flag
"""
test.run_built_executable('cxxflags', stdout=expect)

test.pass_test()
