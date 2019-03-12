#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that pulling in a dependency a second time in a conditional works for
shared_library targets. Regression test for http://crbug.com/122588
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('self_dependency.gyp')

# If running gyp worked, all is well.
test.pass_test()
