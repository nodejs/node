#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that postbuild steps work.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='postbuilds')

  test.build('test.gyp', test.ALL, chdir='postbuilds')

  # See comment in test/subdirectory/gyptest-subdir-default.py
  if test.format == 'xcode':
    chdir = 'postbuilds/subdirectory'
  else:
    chdir = 'postbuilds'

  # Created by the postbuild scripts
  test.built_file_must_exist('el.a_touch',
                             type=test.STATIC_LIB,
                             chdir='postbuilds')
  test.built_file_must_exist('el.a_gyp_touch',
                             type=test.STATIC_LIB,
                             chdir='postbuilds')
  test.built_file_must_exist('nest_el.a_touch',
                             type=test.STATIC_LIB,
                             chdir=chdir)
  test.built_file_must_exist(
      'dyna.framework/Versions/A/dyna_touch',
      chdir='postbuilds')
  test.built_file_must_exist(
      'dyna.framework/Versions/A/dyna_gyp_touch',
      chdir='postbuilds')
  test.built_file_must_exist(
      'nest_dyna.framework/Versions/A/nest_dyna_touch',
      chdir=chdir)
  test.built_file_must_exist('dyna_standalone.dylib_gyp_touch',
                             type=test.SHARED_LIB,
                             chdir='postbuilds')

  test.pass_test()
