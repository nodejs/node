#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies use of include_dirs when using an explicit build target of 'all'.
"""

import TestGyp

test = TestGyp.TestGyp()

if test.format == 'scons':
  test.skip_test('TODO: http://code.google.com/p/gyp/issues/detail?id=176\n')

test.run_gyp('includes.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('includes.gyp', test.ALL, chdir='relocate/src')

expect = """\
Hello from includes.c
Hello from inc.h
Hello from include1.h
Hello from subdir/inc2/include2.h
Hello from shadow2/shadow.h
"""
test.run_built_executable('includes', stdout=expect, chdir='relocate/src')

if test.format == 'xcode':
  chdir='relocate/src/subdir'
else:
  chdir='relocate/src'

expect = """\
Hello from subdir/subdir_includes.c
Hello from subdir/inc.h
Hello from include1.h
Hello from subdir/inc2/include2.h
"""
test.run_built_executable('subdir_includes', stdout=expect, chdir=chdir)

test.pass_test()
