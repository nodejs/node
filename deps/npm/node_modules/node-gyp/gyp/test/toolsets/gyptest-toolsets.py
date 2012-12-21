#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that toolsets are correctly applied
"""

import TestGyp

# Multiple toolsets are currently only supported by the make generator.
test = TestGyp.TestGyp(formats=['make'])

test.run_gyp('toolsets.gyp')

test.build('toolsets.gyp', test.ALL)

test.run_built_executable('host-main', stdout="Host\n")
test.run_built_executable('target-main', stdout="Target\n")

test.pass_test()
