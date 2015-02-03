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
  for platform in ['Win32', 'x64']:
    test.set_configuration('Debug|%s' % platform)
    test.build('basic-idl.gyp', test.ALL, chdir=CHDIR)

    # Make sure ninja win_tool.py filters out noisy lines.
    if test.format == 'ninja' and 'Processing' in test.stdout():
      test.fail_test()

    test.pass_test()
