#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies building a target with nothing succeeds.
"""

import os
import sys
import TestGyp

test = TestGyp.TestGyp()
test.run_gyp('empty-target.gyp')
test.build('empty-target.gyp', target='empty_target')
test.pass_test()
