#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies builds are the same even with different PYTHONHASHSEEDs.
Tests target_short_names and FlattenToList.
"""

import os
import sys
import TestGyp

test = TestGyp.TestGyp()
if test.format == 'ninja':
  os.environ["PYTHONHASHSEED"] = "1"
  test.run_gyp('determinism.gyp')
  base = open(test.built_file_path('build.ninja')).read()

  for i in range(1,5):
    os.environ["PYTHONHASHSEED"] = str(i)
    test.run_gyp('determinism.gyp')
    contents = open(test.built_file_path('build.ninja')).read()
    if base != contents:
      test.fail_test()

  del os.environ["PYTHONHASHSEED"]
  test.pass_test()
