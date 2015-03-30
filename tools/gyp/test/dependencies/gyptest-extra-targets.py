#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that dependencies don't pull unused targets into the build.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('extra_targets.gyp',
             '-G', 'xcode_ninja_target_pattern=^a$')

# This should fail if it tries to build 'c_unused' since 'c/c.c' has a syntax
# error and won't compile.
test.build('extra_targets.gyp', test.ALL)

test.pass_test()
