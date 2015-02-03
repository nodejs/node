#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a dependency on two gyp files with the same name do not create a
uid collision in the resulting generated xcode file.
"""

import TestGyp

import sys

test = TestGyp.TestGyp()

test.run_gyp('test.gyp', chdir='library')

test.pass_test()
