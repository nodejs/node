#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that flat solutions get generated for Express versions of
Visual Studio.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['msvs'])

test.run_gyp('express.gyp', '-G', 'msvs_version=2005')
test.must_contain('express.sln', '(base)')

test.run_gyp('express.gyp', '-G', 'msvs_version=2008')
test.must_contain('express.sln', '(base)')

test.run_gyp('express.gyp', '-G', 'msvs_version=2005e')
test.must_not_contain('express.sln', '(base)')

test.run_gyp('express.gyp', '-G', 'msvs_version=2008e')
test.must_not_contain('express.sln', '(base)')


test.pass_test()
