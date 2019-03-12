#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that the implicit rpath is added only when needed.
"""

import TestGyp

import sys

if sys.platform.startswith('linux'):
  test = TestGyp.TestGyp(formats=['ninja', 'make'])

  CHDIR = 'implicit-rpath'
  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', test.ALL, chdir=CHDIR)

  expect = '$ORIGIN/lib/' if test.format == 'ninja' else '$ORIGIN/lib.target/'

  shared_executable = test.run_readelf('shared_executable', CHDIR)
  if shared_executable != [expect]:
    print('shared_executable=%s' % shared_executable)
    print('expect=%s' % expect)
    test.fail_test()

  shared_executable_no_so_suffix = test.run_readelf('shared_executable_no_so_suffix', CHDIR)
  if shared_executable_no_so_suffix != [expect]:
    test.fail_test()

  static_executable = test.run_readelf('static_executable', CHDIR)
  if static_executable:
    test.fail_test()

  test.pass_test()
