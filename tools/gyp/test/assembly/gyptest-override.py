#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure that manual rules on Windows override the built in ones.
"""

import sys
import TestGyp

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])
  CHDIR = 'src'
  test.run_gyp('override.gyp', chdir=CHDIR)
  test.build('override.gyp', test.ALL, chdir=CHDIR)
  expect = """\
Hello from program.c
Got 42.
"""
  test.run_built_executable('program', chdir=CHDIR, stdout=expect)
  test.pass_test()
