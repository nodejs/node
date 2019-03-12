#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure paths are normalized with VS macros properly expanded on Windows.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  test.run_gyp('normalize-paths.gyp')

  # We can't use existence tests because any case will pass, so we check the
  # contents of ninja files directly since that's what we're most concerned
  # with anyway.
  subninja = open(test.built_file_path('obj/some_target.ninja')).read()
  if '$!product_dir' in subninja:
    test.fail_test()
  if 'out\\Default' in subninja:
    test.fail_test()

  second = open(test.built_file_path('obj/second.ninja')).read()
  if ('..\\..\\things\\AnotherName.exe' in second or
      'AnotherName.exe' not in second):
    test.fail_test()

  copytarget = open(test.built_file_path('obj/copy_target.ninja')).read()
  if '$(VSInstallDir)' in copytarget:
    test.fail_test()

  action = open(test.built_file_path('obj/action.ninja')).read()
  if '..\\..\\out\\Default' in action:
    test.fail_test()
  if '..\\..\\SomethingElse' in action or 'SomethingElse' not in action:
    test.fail_test()
  if '..\\..\\SomeOtherInput' in action or 'SomeOtherInput' not in action:
    test.fail_test()

  test.pass_test()
