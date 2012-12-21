#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Verifies that the user can override the compiler and linker using CC/CXX/LD
environment variables.
"""

import TestGyp
import os
import copy
import sys

here = os.path.dirname(os.path.abspath(__file__))

if sys.platform == 'win32':
  # cross compiling not support by ninja on windows
  # and make not supported on windows at all.
  sys.exit(0)

test = TestGyp.TestGyp(formats=['ninja', 'make'])

def CheckCompiler(test, gypfile, check_for):
  test.run_gyp(gypfile)
  test.build(gypfile)

  # We can't test to presence of my_ld.py in the output since
  # ninja will use CXX_target as the linker regardless
  test.must_contain_all_lines(test.stdout(), check_for)

oldenv = os.environ.copy()
try:
  # Check that CC, CXX and LD set target compiler
  os.environ['CC'] = 'python %s/my_cc.py FOO' % here
  os.environ['CXX'] = 'python %s/my_cxx.py FOO' % here
  os.environ['LD'] = 'python %s/my_ld.py FOO_LINK' % here
  CheckCompiler(test, 'compiler.gyp',
                ['my_cc.py', 'my_cxx.py', 'FOO', 'FOO_LINK'])
finally:
  os.environ.clear()
  os.environ.update(oldenv)

try:
  # Check that CC_host sets host compilee
  os.environ['CC_host'] = 'python %s/my_cc.py HOST' % here
  os.environ['CXX_host'] = 'python %s/my_cxx.py HOST' % here
  os.environ['LD_host'] = 'python %s/my_ld.py HOST_LINK' % here
  CheckCompiler(test, 'compiler-host.gyp',
                ['my_cc.py', 'my_cxx.py', 'HOST', 'HOST_LINK'])
finally:
  os.environ.clear()
  os.environ.update(oldenv)

test.pass_test()
