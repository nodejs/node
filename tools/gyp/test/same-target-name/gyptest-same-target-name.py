#!/usr/bin/env python

# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Check that duplicate targets in a directory gives an error.
"""

import TestGyp

test = TestGyp.TestGyp()

# Require that gyp files with duplicate targets spit out an error.
test.run_gyp('all.gyp', chdir='src', status=1, stderr=None)

test.pass_test()
