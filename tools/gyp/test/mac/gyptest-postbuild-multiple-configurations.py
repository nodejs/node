#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a postbuild work in projects with multiple configurations.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'postbuild-multiple-configurations'
  test.run_gyp('test.gyp', chdir=CHDIR)

  for configuration in ['Debug', 'Release']:
    test.set_configuration(configuration)
    test.build('test.gyp', test.ALL, chdir=CHDIR)
    test.built_file_must_exist('postbuild-file', chdir=CHDIR)

  test.pass_test()
