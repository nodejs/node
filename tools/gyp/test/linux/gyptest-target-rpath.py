#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Check target_rpath generator flag for ninja.
"""

import TestGyp

import re
import subprocess
import sys

if sys.platform.startswith('linux'):
  test = TestGyp.TestGyp(formats=['ninja'])

  CHDIR = 'target-rpath'
  test.run_gyp('test.gyp', '-G', 'target_rpath=/usr/lib/gyptest/', chdir=CHDIR)
  test.build('test.gyp', test.ALL, chdir=CHDIR)

  expect = '/usr/lib/gyptest/'

  if test.run_readelf('shared_executable', chdir=CHDIR) != [expect]:
    test.fail_test()

  if test.run_readelf('shared_executable_no_so_suffix', chdir=CHDIR) != [expect]:
    test.fail_test()

  if test.run_readelf('static_executable', chdir=CHDIR):
    test.fail_test()

  test.pass_test()
