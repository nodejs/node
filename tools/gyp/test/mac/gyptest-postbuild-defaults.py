#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a postbuild invoking |defaults| works.
"""

from __future__ import print_function

import TestGyp

import sys

if sys.platform == 'darwin':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'postbuild-defaults'
  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', test.ALL, chdir=CHDIR)

  result_file = test.built_file_path('result', chdir=CHDIR)
  test.must_exist(result_file)
  test.must_contain(result_file, '''\
Test
${PRODUCT_NAME}
''')

  test.pass_test()
