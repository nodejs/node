#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that stripping works.
"""

import TestGyp

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
    proc = subprocess.Popen(['otool', '-l', p], stdout=subprocess.PIPE)
    o = proc.communicate()[0]
    assert not proc.returncode
    m = r.search(o)
    n = int(m.group(1))
    if n != n_expected:
      print 'Stripping: Expected %d symbols, got %d' % (n_expected, n)
      test.fail_test()

  # The actual numbers here are not interesting, they just need to be the same
  # in both the xcode and the make build.
  CheckNsyms(OutPath('no_postprocess'), 11)
  CheckNsyms(OutPath('no_strip'), 11)
  CheckNsyms(OutPath('strip_all'), 0)
  CheckNsyms(OutPath('strip_nonglobal'), 2)
  CheckNsyms(OutPath('strip_debugging'), 3)
  CheckNsyms(OutPath('strip_all_custom_flags'), 0)
  CheckNsyms(test.built_file_path(
      'strip_all_bundle.framework/Versions/A/strip_all_bundle', chdir='strip'),
      0)
  CheckNsyms(OutPath('strip_save'), 3)

  test.pass_test()
