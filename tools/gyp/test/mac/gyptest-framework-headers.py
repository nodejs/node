#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that mac_framework_headers works properly.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  # TODO(thakis): Make this work with ninja, make. http://crbug.com/129013
  test = TestGyp.TestGyp(formats=['xcode'])

  CHDIR = 'framework-headers'
  test.run_gyp('test.gyp', chdir=CHDIR)

  # Test that headers are installed for frameworks
  test.build('test.gyp', 'test_framework_headers_framework', chdir=CHDIR)

  test.built_file_must_exist(
    'TestFramework.framework/Versions/A/TestFramework', chdir=CHDIR)

  test.built_file_must_exist(
    'TestFramework.framework/Versions/A/Headers/myframework.h', chdir=CHDIR)

  # Test that headers are installed for static libraries.
  test.build('test.gyp', 'test_framework_headers_static', chdir=CHDIR)

  test.built_file_must_exist('libTestLibrary.a', chdir=CHDIR)

  test.built_file_must_exist('include/myframework.h', chdir=CHDIR)

  test.pass_test()
