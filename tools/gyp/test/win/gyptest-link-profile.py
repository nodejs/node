#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that the 'Profile' attribute in VCLinker is extracted properly.
"""

import TestGyp

import os
import sys


if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])
  CHDIR = 'linker-flags'
  test.run_gyp('profile.gyp', chdir=CHDIR)
  test.build('profile.gyp', test.ALL, chdir=CHDIR)

  def GetSummary(exe):
    full_path = test.built_file_path(exe, chdir=CHDIR)
    return test.run_dumpbin(full_path)

  # '.idata' section will be missing when /PROFILE is enabled.
  if '.idata' in GetSummary('test_profile_true.exe'):
    test.fail_test()

  if not '.idata' in GetSummary('test_profile_false.exe'):
    test.fail_test()

  if not '.idata' in GetSummary('test_profile_default.exe'):
    test.fail_test()

  test.pass_test()
