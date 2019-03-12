#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that the xcode-ninja GYP_GENERATOR runs and builds correctly.
"""

import TestGyp

import os
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['xcode'])

  # Run ninja and xcode-ninja
  test.formats = ['ninja', 'xcode-ninja']
  test.run_gyp('test.gyp', chdir='app-bundle')

  # If it builds the target, it works.
  test.build('test.ninja.gyp', chdir='app-bundle')
  test.pass_test()
