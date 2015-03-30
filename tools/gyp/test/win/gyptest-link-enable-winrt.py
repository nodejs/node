#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure msvs_enable_winrt works correctly.
"""

import TestGyp

import os
import sys
import struct

CHDIR = 'enable-winrt'

print 'This test is not currently working on the bots: https://code.google.com/p/gyp/issues/detail?id=466'
sys.exit(0)

if (sys.platform == 'win32' and
    int(os.environ.get('GYP_MSVS_VERSION', 0)) >= 2013):
  test = TestGyp.TestGyp(formats=['msvs'])

  test.run_gyp('enable-winrt.gyp', chdir=CHDIR)

  test.build('enable-winrt.gyp', 'enable_winrt_dll', chdir=CHDIR)

  test.build('enable-winrt.gyp', 'enable_winrt_missing_dll', chdir=CHDIR,
             status=1)

  test.build('enable-winrt.gyp', 'enable_winrt_winphone_dll', chdir=CHDIR)

  test.pass_test()
