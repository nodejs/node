#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies make_global_settings works with the full toolchain.
"""

import os
import sys
import TestGyp

if sys.platform == 'win32':
  # cross compiling not supported by ninja on windows
  # and make not supported on windows at all.
  sys.exit(0)

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

gyp_file = 'make_global_settings.gyp'

test.run_gyp(gyp_file,
             # Teach the .gyp file about the location of my_nm.py and
             # my_readelf.py, and the python executable.
             '-Dworkdir=%s' % test.workdir,
             '-Dpython=%s' % sys.executable)
test.build(gyp_file,
           arguments=['-v'] if test.format == 'ninja-my_flavor' else [])

expected = ['MY_CC', 'MY_CXX']
test.must_contain_all_lines(test.stdout(), expected)

test.must_contain(test.built_file_path('RAN_MY_NM'), 'RAN_MY_NM')
test.must_contain(test.built_file_path('RAN_MY_READELF'), 'RAN_MY_READELF')

test.pass_test()
