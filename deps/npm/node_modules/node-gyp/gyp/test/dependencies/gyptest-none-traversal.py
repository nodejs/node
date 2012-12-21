#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that static library dependencies don't traverse none targets, unless
explicitly specified.
"""

import TestGyp

import sys

test = TestGyp.TestGyp()

test.run_gyp('none_traversal.gyp')

test.build('none_traversal.gyp', test.ALL)

test.run_built_executable('needs_chain', stdout="2\n")
test.run_built_executable('doesnt_need_chain', stdout="3\n")

test.pass_test()
