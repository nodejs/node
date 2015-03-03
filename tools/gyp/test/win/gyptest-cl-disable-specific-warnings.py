#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure disable specific warnings is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('disable-specific-warnings.gyp', chdir=CHDIR)

  # The source file contains a warning, so if WarnAsError is true and
  # DisableSpecificWarnings for the warning in question is set, then the build
  # should succeed, otherwise it must fail.

  test.build('disable-specific-warnings.gyp',
             'test_disable_specific_warnings_set',
             chdir=CHDIR)
  test.build('disable-specific-warnings.gyp',
             'test_disable_specific_warnings_unset',
             chdir=CHDIR, status=1)

  test.pass_test()
