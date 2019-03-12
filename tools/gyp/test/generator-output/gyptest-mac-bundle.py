#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies mac bundles work with --generator-output.
"""

from __future__ import print_function

import TestGyp

import sys

if sys.platform == 'darwin':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

  test = TestGyp.TestGyp(formats=[])

  MAC_BUNDLE_DIR = 'mac-bundle'
  GYPFILES_DIR = 'gypfiles'
  test.writable(test.workpath(MAC_BUNDLE_DIR), False)
  test.run_gyp('test.gyp',
               '--generator-output=' + test.workpath(GYPFILES_DIR),
               chdir=MAC_BUNDLE_DIR)
  test.writable(test.workpath(MAC_BUNDLE_DIR), True)

  test.build('test.gyp', test.ALL, chdir=GYPFILES_DIR)

  test.pass_test()
