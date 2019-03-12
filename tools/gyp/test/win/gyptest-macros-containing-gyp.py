#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Handle VS macro expansion containing gyp variables.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'vs-macros'
  test.run_gyp('containing-gyp.gyp', chdir=CHDIR)
  test.build('containing-gyp.gyp', test.ALL, chdir=CHDIR)
  test.pass_test()
