#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure TreatWChar_tAsBuiltInType option is functional.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('treat-wchar-t-as-built-in-type.gyp', chdir=CHDIR)
  test.build('treat-wchar-t-as-built-in-type.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
