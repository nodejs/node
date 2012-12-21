#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure RTC setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('runtime-checks.gyp', chdir=CHDIR)

  # Runtime checks disabled, should fail.
  test.build('runtime-checks.gyp', 'test_brc_none', chdir=CHDIR, status=1)

  # Runtime checks enabled, should pass.
  test.build('runtime-checks.gyp', 'test_brc_1', chdir=CHDIR)

  # TODO(scottmg): There are other less frequently used/partial options, but
  # it's not clear how to verify them, so ignore for now.

  test.pass_test()
