#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies actions with multiple outputs will correctly rebuild.
"""

from __future__ import print_function

import TestGyp
import os
import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

test = TestGyp.TestGyp()

test.run_gyp('multiple-outputs.gyp', chdir='src')

chdir = 'relocate/src'
test.relocate('src', chdir)

def build_and_check():
  # Build + check that both outputs exist.
  test.build('multiple-outputs.gyp', chdir=chdir)
  test.built_file_must_exist('out1.txt', chdir=chdir)
  test.built_file_must_exist('out2.txt', chdir=chdir)

# Plain build.
build_and_check()

# Remove either + rebuild. Both should exist (again).
os.remove(test.built_file_path('out1.txt', chdir=chdir))
build_and_check();

# Remove the other + rebuild. Both should exist (again).
os.remove(test.built_file_path('out2.txt', chdir=chdir))
build_and_check();

test.pass_test()
