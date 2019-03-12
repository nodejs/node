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
if (sys.platform != 'win32' and
    not (sys.platform == 'darwin' and test.format == 'make')):
  # TODO: Does a test like this make sense with Windows?

  CHDIR = 'src'
  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', 'no_cpp', chdir=CHDIR)

  def LinksLibStdCpp(path):
    path = test.built_file_path(path, chdir=CHDIR)
    if sys.platform == 'darwin':
      proc = subprocess.Popen(['otool', '-L', path], stdout=subprocess.PIPE)
    else:
      proc = subprocess.Popen(['ldd', path], stdout=subprocess.PIPE)
    output = proc.communicate()[0].decode('utf-8')
    assert not proc.returncode
    return 'libstdc++' in output or 'libc++' in output

  if LinksLibStdCpp('no_cpp'):
    test.fail_test()

  # Make, ninja, and CMake pick the compiler driver based on transitive
  # checks. Xcode doesn't.
  build_error_code = {
    'xcode': 65,  # EX_DATAERR, see `man sysexits`
    'make': 0,
    'ninja': 0,
    'cmake': 0,
    'xcode-ninja': 0,
  }[test.format]

  test.build('test.gyp', 'no_cpp_dep_on_cc_lib', chdir=CHDIR,
             status=build_error_code)

  test.pass_test()
