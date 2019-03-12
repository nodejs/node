#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test that .idl files in actions and non-native rules are excluded.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'idl-excluded'
  test.run_gyp('idl-excluded.gyp', chdir=CHDIR)
  test.build('idl-excluded.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
