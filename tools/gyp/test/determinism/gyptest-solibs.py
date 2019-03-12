#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies builds are the same even with different PYTHONHASHSEEDs.
Tests all_targets, implicit_deps and solibs.
"""

from __future__ import print_function

import os
import sys
import TestGyp

test = TestGyp.TestGyp()
if test.format == 'ninja':
  os.environ["PYTHONHASHSEED"] = "1"
  test.run_gyp('solibs.gyp')
  base1 = open(test.built_file_path('c.ninja', subdir='obj')).read()
  base2 = open(test.built_file_path('build.ninja')).read()

  for i in range(1,5):
    os.environ["PYTHONHASHSEED"] = str(i)
    test.run_gyp('solibs.gyp')
    contents1 = open(test.built_file_path('c.ninja', subdir='obj')).read()
    contents2 = open(test.built_file_path('build.ninja')).read()
    if base1 != contents1:
      test.fail_test()
    if base2 != contents2:
      print(base2)
      test.fail_test()

  del os.environ["PYTHONHASHSEED"]
  test.pass_test()
