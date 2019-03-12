#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure PREfast (code analysis) setting is extracted properly.
"""

import TestGyp

import os
import sys

if (sys.platform == 'win32' and
    int(os.environ.get('GYP_MSVS_VERSION', 0)) >= 2012):
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('analysis.gyp', chdir=CHDIR)

  # Analysis enabled, should fail.
  test.build('analysis.gyp', 'test_analysis_on', chdir=CHDIR, status=1)

  # Analysis not enabled, or unspecified, should pass.
  test.build('analysis.gyp', 'test_analysis_off', chdir=CHDIR)
  test.build('analysis.gyp', 'test_analysis_unspec', chdir=CHDIR)

  test.pass_test()
