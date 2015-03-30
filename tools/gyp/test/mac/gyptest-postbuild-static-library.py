#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a postbuilds on static libraries work, and that sourceless
libraries don't cause failures at gyp time.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['make', 'xcode'])

  CHDIR = 'postbuild-static-library'
  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', 'my_lib', chdir=CHDIR)
  # Building my_sourceless_lib doesn't work with make. gyp should probably
  # forbid sourceless static libraries, since they're pretty pointless.
  # But they shouldn't cause gyp time exceptions.

  test.built_file_must_exist('postbuild-file', chdir=CHDIR)

  test.pass_test()
