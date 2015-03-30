#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that a target marked as 'link_dependency==1' isn't being pulled into
the 'none' target's dependency (which would otherwise lead to a dependency
cycle in ninja).
"""

import TestGyp

# See https://codereview.chromium.org/177043010/#msg15 for why this doesn't
# work with cmake.
test = TestGyp.TestGyp(formats=['!cmake'])

test.run_gyp('test.gyp')
test.build('test.gyp', 'main')

# If running gyp worked, all is well.
test.pass_test()
