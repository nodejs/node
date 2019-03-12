#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Build a .gyp with two targets that share a common .c source file.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('all.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('all.gyp', chdir='relocate/src')

expect1 = """\
Hello from prog1.c
Hello prog1 from func.c
"""

expect2 = """\
Hello from prog2.c
Hello prog2 from func.c
"""

test.run_built_executable('prog1', chdir='relocate/src', stdout=expect1)
test.run_built_executable('prog2', chdir='relocate/src', stdout=expect2)

test.pass_test()
