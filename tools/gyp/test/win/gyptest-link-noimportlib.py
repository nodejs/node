#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure that the (custom) NoImportLibrary flag is handled correctly.
"""

import TestGyp

import os
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  CHDIR = 'importlib'
  test.run_gyp('noimplib.gyp', chdir=CHDIR)
  test.build('noimplib.gyp', test.ALL, chdir=CHDIR)

  # The target has an entry point, but no exports. Ordinarily, ninja expects
  # all DLLs to export some symbols (with the exception of /NOENTRY resource-
  # only DLLs). When the NoImportLibrary flag is set, this is suppressed. If
  # this is not working correctly, the expected .lib will never be generated
  # but will be expected, so the build will not be up to date.
  test.up_to_date('noimplib.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
