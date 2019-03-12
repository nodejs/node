#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure DefaultCharIsUnsigned option is functional.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('default-char-is-unsigned.gyp', chdir=CHDIR)
  test.build('default-char-is-unsigned.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
