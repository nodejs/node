#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verify that a link time only dependency will get pulled into the set of built
targets, even if no executable uses it.
"""

import TestGyp

import sys

test = TestGyp.TestGyp()

test.run_gyp('lib_only.gyp')

test.build('lib_only.gyp', test.ALL)

test.built_file_must_exist('a', type=test.STATIC_LIB)

# TODO(bradnelson/mark):
# On linux and windows a library target will at least pull its link dependencies
# into the generated sln/_main.scons, since not doing so confuses users.
# This is not currently implemented on mac, which has the opposite behavior.
if sys.platform == 'darwin':
  if test.format == 'xcode':
    test.built_file_must_not_exist('b', type=test.STATIC_LIB)
  else:
    assert test.format in ('make', 'ninja')
    test.built_file_must_exist('b', type=test.STATIC_LIB)
else:
  # Make puts the resulting library in a directory matching the input gyp file;
  # for the 'b' library, that is in the 'b' subdirectory.
  test.built_file_must_exist('b', type=test.STATIC_LIB, subdir='b')

test.pass_test()
