#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure warning-as-error is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('warning-as-error.gyp', chdir=CHDIR)

  # The source file contains a warning, so if WarnAsError is false (or
  # default, which is also false), then the build should succeed, otherwise it
  # must fail.

  test.build('warning-as-error.gyp', 'test_warn_as_error_false', chdir=CHDIR)
  test.build('warning-as-error.gyp', 'test_warn_as_error_unset', chdir=CHDIR)
  test.build('warning-as-error.gyp', 'test_warn_as_error_true', chdir=CHDIR,
    status=1)

  test.pass_test()
