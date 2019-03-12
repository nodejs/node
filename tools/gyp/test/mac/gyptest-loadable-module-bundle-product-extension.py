#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests that loadable_modules don't collide when using the same name with
different file extensions.
"""

import TestGyp

import os
import struct
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'loadable-module-bundle-product-extension'
  test.run_gyp('test.gyp',
               '-G', 'xcode_ninja_target_pattern=^.*$',
               chdir=CHDIR)

  test.build('test.gyp', test.ALL, chdir=CHDIR)

  test.must_exist(test.built_file_path('Collide.foo', chdir=CHDIR))
  test.must_exist(test.built_file_path('Collide.bar', chdir=CHDIR))

  test.pass_test()
