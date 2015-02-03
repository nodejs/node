#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Verifies that the user can override the compiler and linker using
CC/CXX/NM/READELF environment variables.
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
for key in ['CC', 'CXX', 'LINK', 'CC_host', 'CXX_host', 'LINK_host',
            'NM_target', 'READELF_target']:
  if key in os.environ:
    del os.environ[key]


def CheckCompiler(test, gypfile, check_for, run_gyp):
  if run_gyp:
    test.run_gyp(gypfile)
  test.build(gypfile)

  test.must_contain_all_lines(test.stdout(), check_for)


test = TestGyp.TestGyp(formats=['ninja'])
# Must set the test format to something with a flavor (the part after the '-')
# in order to test the desired behavior. Since we want to run a non-host
# toolchain, we have to set the flavor to something that the ninja generator
# doesn't know about, so it doesn't default to the host-specific tools (e.g.,
# 'otool' on mac to generate the .TOC).
#
# Note that we can't just pass format=['ninja-some_toolchain'] to the
# constructor above, because then this test wouldn't be recognized as a ninja
# format test.
test.formats = ['ninja-my_flavor' if f == 'ninja' else f for f in test.formats]


def TestTargetOverideSharedLib():
  # The std output from nm and readelf is redirected to files, so we can't
  # expect their output to appear. Instead, check for the files they create to
  # see if they actually ran.
  expected = ['my_cc.py', 'my_cxx.py', 'FOO']

  # Check that CC, CXX, NM, READELF, set target compiler
  env = {'CC': 'python %s/my_cc.py FOO' % here,
         'CXX': 'python %s/my_cxx.py FOO' % here,
         'NM': 'python %s/my_nm.py' % here,
         'READELF': 'python %s/my_readelf.py' % here}

  with TestGyp.LocalEnv(env):
    CheckCompiler(test, 'compiler-shared-lib.gyp', expected, True)
    test.must_contain(test.built_file_path('RAN_MY_NM'), 'RAN_MY_NM')
    test.must_contain(test.built_file_path('RAN_MY_READELF'), 'RAN_MY_READELF')
    test.unlink(test.built_file_path('RAN_MY_NM'))
    test.unlink(test.built_file_path('RAN_MY_READELF'))

  # Run the same tests once the eviron has been restored.  The generated
  # projects should have embedded all the settings in the project files so the
  # results should be the same.
  CheckCompiler(test, 'compiler-shared-lib.gyp', expected, False)
  test.must_contain(test.built_file_path('RAN_MY_NM'), 'RAN_MY_NM')
  test.must_contain(test.built_file_path('RAN_MY_READELF'), 'RAN_MY_READELF')


TestTargetOverideSharedLib()
test.pass_test()
