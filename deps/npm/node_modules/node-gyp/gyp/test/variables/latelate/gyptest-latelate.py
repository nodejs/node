#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ^(latelate) style variables work.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('latelate.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('latelate.gyp', test.ALL, chdir='relocate/src')

test.run_built_executable(
    'program', chdir='relocate/src', stdout='program.cc\n')


test.pass_test()
