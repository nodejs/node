#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that toolsets are correctly applied
"""
import os
import sys
import TestGyp

if sys.platform.startswith('linux'):

  test = TestGyp.TestGyp(formats=['make', 'ninja'])

  oldenv = os.environ.copy()
  try:
    os.environ['GYP_CROSSCOMPILE'] = '1'
    test.run_gyp('toolsets.gyp')
  finally:
    os.environ.clear()
    os.environ.update(oldenv)

  test.build('toolsets.gyp', test.ALL)

  test.run_built_executable('host-main', stdout="Host\nShared: Host\n")
  test.run_built_executable('target-main', stdout="Target\nShared: Target\n")

  test.pass_test()
