#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test that custom generators can be passed to --format
"""

import TestGyp

test = TestGyp.TestGypCustom(format='mygenerator.py')
test.run_gyp('test.gyp')

# mygenerator.py should generate a file called MyBuildFile containing
# "Testing..." alongside the gyp file.
test.must_match('MyBuildFile', 'Testing...\n')

test.pass_test()
