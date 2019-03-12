#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test that empty variable names don't cause infinite loops.
"""

import os

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('empty.gyp')

test.pass_test()
