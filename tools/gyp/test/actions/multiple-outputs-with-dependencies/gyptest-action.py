#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies actions with multiple outputs & dependncies will correctly rebuild.

This is a regression test for crrev.com/1177163002.
"""

from __future__ import print_function

import TestGyp
import os
import sys
import time

if sys.platform in ('darwin', 'win32'):
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

test = TestGyp.TestGyp()

TESTDIR='relocate/src'
test.run_gyp('action.gyp', chdir='src')
test.relocate('src', TESTDIR)

def build_and_check(content):
  test.write(TESTDIR + '/input.txt', content)
  test.build('action.gyp', 'upper', chdir=TESTDIR)
  test.built_file_must_match('result.txt', content, chdir=TESTDIR)

build_and_check('Content for first build.')

# Ninja works with timestamps and the test above is fast enough that the
# 'updated' file may end up with the same timestamp as the original, meaning
# that ninja may not always recognize the input file has changed.
if test.format == 'ninja':
  time.sleep(1)

build_and_check('An updated input file.')

test.pass_test()
