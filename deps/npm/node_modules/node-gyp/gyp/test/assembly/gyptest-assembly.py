#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A basic test of compiling assembler files.
"""

import sys
import TestGyp

if sys.platform != 'win32':
  # TODO(bradnelson): get this working for windows.
  test = TestGyp.TestGyp(formats=['!msvs'])

  test.run_gyp('assembly.gyp', chdir='src')

  test.relocate('src', 'relocate/src')

  test.build('assembly.gyp', test.ALL, chdir='relocate/src')

  expect = """\
Hello from program.c
Got 42.
"""
  test.run_built_executable('program', chdir='relocate/src', stdout=expect)


  test.pass_test()
