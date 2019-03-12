#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies dependencies do the copy step.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('copies.gyp', chdir='src')

test.build('copies.gyp', 'proj2', chdir='src')

test.run_built_executable('proj1',
                          chdir='src',
                          stdout="Hello from file1.c\n")
test.run_built_executable('proj2',
                          chdir='src',
                          stdout="Hello from file2.c\n")

test.pass_test()
