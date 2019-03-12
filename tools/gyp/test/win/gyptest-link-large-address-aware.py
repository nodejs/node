#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure largeaddressaware setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('large-address-aware.gyp', chdir=CHDIR)
  test.build('large-address-aware.gyp', test.ALL, chdir=CHDIR)

  def GetHeaders(exe):
    return test.run_dumpbin('/headers', test.built_file_path(exe, chdir=CHDIR))

  MARKER = 'Application can handle large (>2GB) addresses'

  # Explicitly off.
  if MARKER in GetHeaders('test_large_address_aware_no.exe'):
    test.fail_test()

  # Explicitly on.
  if MARKER not in GetHeaders('test_large_address_aware_yes.exe'):
    test.fail_test()

  test.pass_test()
