#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that actions can be + a source scanner can be used to implement,
cross-compiles (for Native Client at this point).
"""

import TestGyp

test = TestGyp.TestGyp()

# TODO(bradnelson): fix scons.
if test.format == 'scons':
  test.skip_test()

test.run_gyp('cross.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('cross.gyp', test.ALL, chdir='relocate/src')

expect = """\
From test1.cc
From test2.c
From test3.cc
From test4.c
"""
test.run_built_executable('program', chdir='relocate/src', stdout=expect)


test.pass_test()
