#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Checks that C-only targets aren't linked against libstdc++.
"""

import TestGyp

import re
import subprocess
import sys

# set |match| to ignore build stderr output.
test = TestGyp.TestGyp(match = lambda a, b: True)
if sys.platform != 'win32' and test.format not in ('make', 'android'):
  # TODO: This doesn't pass with make.
  # TODO: Does a test like this make sense with Windows? Android?

  CHDIR = 'src'
  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', 'no_cpp', chdir=CHDIR)

  def LinksLibStdCpp(path):
    path = test.built_file_path(path, chdir=CHDIR)
    if sys.platform == 'darwin':
      proc = subprocess.Popen(['otool', '-L', path], stdout=subprocess.PIPE)
    else:
      proc = subprocess.Popen(['ldd', path], stdout=subprocess.PIPE)
    output = proc.communicate()[0]
    assert not proc.returncode
    return 'libstdc++' in output or 'libc++' in output

  if LinksLibStdCpp('no_cpp'):
    test.fail_test()

  build_error_code = {
    'xcode': [1, 65],  # 1 for xcode 3, 65 for xcode 4 (see `man sysexits`)
    'make': 2,
    'ninja': 1,
    'cmake': 0,  # CMake picks the compiler driver based on transitive checks.
    'xcode-ninja': [1, 65],
  }[test.format]

  test.build('test.gyp', 'no_cpp_dep_on_cc_lib', chdir=CHDIR,
             status=build_error_code)

  test.pass_test()
