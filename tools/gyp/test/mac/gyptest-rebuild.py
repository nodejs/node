#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that app bundles are rebuilt correctly.
"""

import TestGyp

import os
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['make', 'xcode'])

  test.run_gyp('test.gyp', chdir='app-bundle')

  test.build('test.gyp', test.ALL, chdir='app-bundle')

  # Touch a source file, rebuild, and check that the app target is up-to-date.
  os.utime('app-bundle/TestApp/main.m', None)
  test.build('test.gyp', test.ALL, chdir='app-bundle')

  test.up_to_date('test.gyp', test.ALL, chdir='app-bundle')

  test.pass_test()
