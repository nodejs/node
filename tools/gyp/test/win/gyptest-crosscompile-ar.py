#!/usr/bin/env python

# Copyright 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ar_host is set correctly when enabling cross-compile on windows.
"""

import TestGyp

import sys
import os

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  CHDIR = 'lib-crosscompile'
  oldenv = os.environ.copy()
  try:
    os.environ['GYP_CROSSCOMPILE'] = '1'
    test.run_gyp('use_host_ar.gyp', chdir=CHDIR)
  finally:
    os.environ.clear()
    os.environ.update(oldenv)

  test.build('use_host_ar.gyp', test.ALL, chdir=CHDIR)
  test.pass_test()
