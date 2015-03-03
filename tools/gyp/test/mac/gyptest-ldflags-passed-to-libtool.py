#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that OTHER_LDFLAGS is passed to libtool.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'],
                         match = lambda a, b: True)

  build_error_code = {
    'xcode': [1, 65],  # 1 for xcode 3, 65 for xcode 4 (see `man sysexits`)
    'make': 2,
    'ninja': 1,
    'xcode-ninja': [1, 65],
  }[test.format]

  CHDIR = 'ldflags-libtool'
  test.run_gyp('test.gyp', chdir=CHDIR)

  test.build('test.gyp', 'ldflags_passed_to_libtool', chdir=CHDIR,
             status=build_error_code)

  test.pass_test()
