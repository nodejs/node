#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure TargetMachine setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('target-machine.gyp', chdir=CHDIR)
  # The .cc file is compiled as x86 (the default), so the link/libs that are
  # x64 need to fail.
  test.build('target-machine.gyp', 'test_target_link_x86', chdir=CHDIR)
  test.build(
      'target-machine.gyp', 'test_target_link_x64', chdir=CHDIR, status=1)
  test.build('target-machine.gyp', 'test_target_lib_x86', chdir=CHDIR)
  test.build('target-machine.gyp', 'test_target_lib_x64', chdir=CHDIR, status=1)

  test.pass_test()
