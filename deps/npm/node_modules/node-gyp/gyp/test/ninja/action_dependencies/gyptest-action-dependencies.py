#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that building an object file correctly depends on running actions in
dependent targets, but not the targets themselves.
"""

import os
import sys
import TestGyp

# NOTE(piman): This test will not work with other generators because:
# - it explicitly tests the optimization, which is not implemented (yet?) on
# other generators
# - it relies on the exact path to output object files, which is generator
# dependent, and actually, relies on the ability to build only that object file,
# which I don't think is available on all generators.
# TODO(piman): Extend to other generators when possible.
test = TestGyp.TestGyp(formats=['ninja'])

test.run_gyp('action_dependencies.gyp', chdir='src')

chdir = 'relocate/src'
test.relocate('src', chdir)

objext = '.obj' if sys.platform == 'win32' else '.o'

test.build('action_dependencies.gyp',
           os.path.join('obj', 'b.b' + objext),
           chdir=chdir)

# The 'a' actions should be run (letting b.c compile), but the a static library
# should not be built.
test.built_file_must_not_exist('a', type=test.STATIC_LIB, chdir=chdir)
test.built_file_must_not_exist('b', type=test.STATIC_LIB, chdir=chdir)
test.built_file_must_exist(os.path.join('obj', 'b.b' + objext), chdir=chdir)

test.build('action_dependencies.gyp',
           os.path.join('obj', 'c.c' + objext),
           chdir=chdir)

# 'a' and 'b' should be built, so that the 'c' action succeeds, letting c.c
# compile
test.built_file_must_exist('a', type=test.STATIC_LIB, chdir=chdir)
test.built_file_must_exist('b', type=test.EXECUTABLE, chdir=chdir)
test.built_file_must_exist(os.path.join('obj', 'c.c' + objext), chdir=chdir)


test.pass_test()
