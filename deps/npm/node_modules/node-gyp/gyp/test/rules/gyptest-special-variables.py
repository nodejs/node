#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Verifies that VS variables that require special variables are expanded
correctly. """

import sys
import TestGyp

if sys.platform == 'win32':
  test = TestGyp.TestGyp()

  test.run_gyp('special-variables.gyp', chdir='src')
  test.build('special-variables.gyp', test.ALL, chdir='src')
  test.pass_test()
