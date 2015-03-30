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
  # cross compiling not supported by ninja on windows
  # and make not supported on windows at all.
  sys.exit(0)

# Clear any existing compiler related env vars.
for key in ['CC', 'CXX', 'LINK', 'CC_host', 'CXX_host', 'LINK_host']:
  if key in os.environ:
    del os.environ[key]


def CheckCompiler(test, gypfile, check_for, run_gyp):
  if run_gyp:
    test.run_gyp(gypfile)
  test.build(gypfile)

  test.must_contain_all_lines(test.stdout(), check_for)


test = TestGyp.TestGyp(formats=['ninja', 'make'])

def TestTargetOveride():
  expected = ['my_cc.py', 'my_cxx.py', 'FOO' ]

  # ninja just uses $CC / $CXX as linker.
  if test.format not in ['ninja', 'xcode-ninja']:
    expected.append('FOO_LINK')

  # Check that CC, CXX and LD set target compiler
  oldenv = os.environ.copy()
  try:
    os.environ['CC'] = 'python %s/my_cc.py FOO' % here
    os.environ['CXX'] = 'python %s/my_cxx.py FOO' % here
    os.environ['LINK'] = 'python %s/my_ld.py FOO_LINK' % here

    CheckCompiler(test, 'compiler-exe.gyp', expected, True)
  finally:
    os.environ.clear()
    os.environ.update(oldenv)

  # Run the same tests once the eviron has been restored.  The
  # generated should have embedded all the settings in the
  # project files so the results should be the same.
  CheckCompiler(test, 'compiler-exe.gyp', expected, False)


def TestTargetOverideCompilerOnly():
  # Same test again but with that CC, CXX and not LD
  oldenv = os.environ.copy()
  try:
    os.environ['CC'] = 'python %s/my_cc.py FOO' % here
    os.environ['CXX'] = 'python %s/my_cxx.py FOO' % here

    CheckCompiler(test, 'compiler-exe.gyp',
                  ['my_cc.py', 'my_cxx.py', 'FOO'],
                  True)
  finally:
    os.environ.clear()
    os.environ.update(oldenv)

  # Run the same tests once the eviron has been restored.  The
  # generated should have embedded all the settings in the
  # project files so the results should be the same.
  CheckCompiler(test, 'compiler-exe.gyp',
                ['my_cc.py', 'my_cxx.py', 'FOO'],
                False)


def TestHostOveride():
  expected = ['my_cc.py', 'my_cxx.py', 'HOST' ]
  if test.format != 'ninja':  # ninja just uses $CC / $CXX as linker.
    expected.append('HOST_LINK')

  # Check that CC_host sets host compilee
  oldenv = os.environ.copy()
  try:
    os.environ['CC_host'] = 'python %s/my_cc.py HOST' % here
    os.environ['CXX_host'] = 'python %s/my_cxx.py HOST' % here
    os.environ['LINK_host'] = 'python %s/my_ld.py HOST_LINK' % here
    CheckCompiler(test, 'compiler-host.gyp', expected, True)
  finally:
    os.environ.clear()
    os.environ.update(oldenv)

  # Run the same tests once the eviron has been restored.  The
  # generated should have embedded all the settings in the
  # project files so the results should be the same.
  CheckCompiler(test, 'compiler-host.gyp', expected, False)


TestTargetOveride()
TestTargetOverideCompilerOnly()

test.pass_test()
