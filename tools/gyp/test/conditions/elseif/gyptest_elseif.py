#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that "else-if" conditions work.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('elseif.gyp')
test.build('elseif.gyp', test.ALL)
test.run_built_executable(
    'program0', stdout='first_if\n')
test.run_built_executable(
    'program1', stdout='first_else_if\n')
test.run_built_executable(
    'program2', stdout='second_else_if\n')
test.run_built_executable(
    'program3', stdout='third_else_if\n')
test.run_built_executable(
    'program4', stdout='last_else\n')

# Verify that bad condition blocks fail at gyp time.
test.run_gyp('elseif_bad1.gyp', status=1, stderr=None)
test.run_gyp('elseif_bad2.gyp', status=1, stderr=None)
test.run_gyp('elseif_bad3.gyp', status=1, stderr=None)

test.pass_test()
