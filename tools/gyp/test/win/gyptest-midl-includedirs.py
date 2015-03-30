#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that 'midl_include_dirs' is handled.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'idl-includedirs'
  test.run_gyp('idl-includedirs.gyp', chdir=CHDIR)
  test.build('idl-includedirs.gyp', test.ALL, chdir=CHDIR)
  test.pass_test()
