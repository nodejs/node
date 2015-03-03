#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that stripping works.
"""

import TestGyp
import TestMac

import re
import subprocess
import sys
import time

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='strip')

  test.build('test.gyp', test.ALL, chdir='strip')

  # Lightweight check if stripping was done.
  def OutPath(s):
    return test.built_file_path(s, type=test.SHARED_LIB, chdir='strip')

  def CheckNsyms(p, n_expected):
    r = re.compile(r'nsyms\s+(\d+)')
    o = subprocess.check_output(['otool', '-l', p])
    m = r.search(o)
    n = int(m.group(1))
    if n != n_expected:
      print 'Stripping: Expected %d symbols, got %d' % (n_expected, n)
      test.fail_test()

  # Starting with Xcode 5.0, clang adds an additional symbols to the compiled
  # file when using a relative path to the input file. So when using ninja
  # with Xcode 5.0 or higher, take this additional symbol into consideration
  # for unstripped builds (it is stripped by all strip commands).
  expected_extra_symbol_count = 0
  if test.format in ['ninja', 'xcode-ninja'] \
      and TestMac.Xcode.Version() >= '0500':
    expected_extra_symbol_count = 1

  # The actual numbers here are not interesting, they just need to be the same
  # in both the xcode and the make build.
  CheckNsyms(OutPath('no_postprocess'), 29 + expected_extra_symbol_count)
  CheckNsyms(OutPath('no_strip'), 29 + expected_extra_symbol_count)
  CheckNsyms(OutPath('strip_all'), 0)
  CheckNsyms(OutPath('strip_nonglobal'), 6)
  CheckNsyms(OutPath('strip_debugging'), 7)
  CheckNsyms(OutPath('strip_all_custom_flags'), 0)
  CheckNsyms(test.built_file_path(
      'strip_all_bundle.framework/Versions/A/strip_all_bundle', chdir='strip'),
      0)
  CheckNsyms(OutPath('strip_save'), 7)

  test.pass_test()
