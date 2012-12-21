#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Handle default .idl build rules.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'idl-rules'
  test.run_gyp('basic-idl.gyp', chdir=CHDIR)
  test.build('basic-idl.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
