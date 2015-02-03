#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests things related to ARCHS.
"""

import TestGyp
import TestMac

import re
import subprocess
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test-no-archs.gyp', chdir='archs')
  test.build('test-no-archs.gyp', test.ALL, chdir='archs')
  result_file = test.built_file_path('Test', chdir='archs')
  test.must_exist(result_file)

  if TestMac.Xcode.Version() >= '0500':
    expected_type = ['x86_64']
  else:
    expected_type = ['i386']
  TestMac.CheckFileType(test, result_file, expected_type)

  test.run_gyp('test-valid-archs.gyp', chdir='archs')
  test.build('test-valid-archs.gyp', test.ALL, chdir='archs')
  result_file = test.built_file_path('Test', chdir='archs')
  test.must_exist(result_file)
  TestMac.CheckFileType(test, result_file, ['x86_64'])

  test.run_gyp('test-archs-x86_64.gyp', chdir='archs')
  test.build('test-archs-x86_64.gyp', test.ALL, chdir='archs')
  result_file = test.built_file_path('Test64', chdir='archs')
  test.must_exist(result_file)
  TestMac.CheckFileType(test, result_file, ['x86_64'])

  test.run_gyp('test-dependencies.gyp', chdir='archs')
  test.build('test-dependencies.gyp', target=test.ALL, chdir='archs')
  products = ['c_standalone', 'd_standalone']
  for product in products:
    result_file = test.built_file_path(
        product, chdir='archs', type=test.STATIC_LIB)
    test.must_exist(result_file)

  if test.format != 'make':
    # Build all targets except 'exe_32_64_no_sources' that does build
    # but should not cause error when generating ninja files
    targets = [
        'static_32_64', 'shared_32_64', 'shared_32_64_bundle',
        'module_32_64', 'module_32_64_bundle',
        'exe_32_64', 'exe_32_64_bundle', 'precompiled_prefix_header_mm_32_64',
    ]

    test.run_gyp('test-archs-multiarch.gyp', chdir='archs')

    for target in targets:
      test.build('test-archs-multiarch.gyp', target=target, chdir='archs')

    result_file = test.built_file_path(
        'static_32_64', chdir='archs', type=test.STATIC_LIB)
    test.must_exist(result_file)
    TestMac.CheckFileType(test, result_file, ['i386', 'x86_64'])

    result_file = test.built_file_path(
        'shared_32_64', chdir='archs', type=test.SHARED_LIB)
    test.must_exist(result_file)
    TestMac.CheckFileType(test, result_file, ['i386', 'x86_64'])

    result_file = test.built_file_path('My Framework.framework/My Framework',
                                       chdir='archs')
    test.must_exist(result_file)
    TestMac.CheckFileType(test, result_file, ['i386', 'x86_64'])
    # Check that symbol "_x" made it into both versions of the binary:
    if not all(['D _x' in subprocess.check_output(
        ['nm', '-arch', arch, result_file]) for arch in ['i386', 'x86_64']]):
      # This can only flakily fail, due to process ordering issues. If this
      # does fail flakily, then something's broken, it's not the test at fault.
      test.fail_test()

    result_file = test.built_file_path(
        'exe_32_64', chdir='archs', type=test.EXECUTABLE)
    test.must_exist(result_file)
    TestMac.CheckFileType(test, result_file, ['i386', 'x86_64'])

    result_file = test.built_file_path('Test App.app/Contents/MacOS/Test App',
                                       chdir='archs')
    test.must_exist(result_file)
    TestMac.CheckFileType(test, result_file, ['i386', 'x86_64'])
