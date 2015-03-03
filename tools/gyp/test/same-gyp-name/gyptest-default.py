#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Build a .gyp that depends on 2 gyp files with the same name.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('all.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('all.gyp', chdir='relocate/src')

expect1 = """\
Hello from main1.cc
"""

expect2 = """\
Hello from main2.cc
"""

if test.format == 'xcode':
  chdir1 = 'relocate/src/subdir1'
  chdir2 = 'relocate/src/subdir2'
else:
  chdir1 = chdir2 = 'relocate/src'

test.run_built_executable('program1', chdir=chdir1, stdout=expect1)
test.run_built_executable('program2', chdir=chdir2, stdout=expect2)

test.pass_test()
