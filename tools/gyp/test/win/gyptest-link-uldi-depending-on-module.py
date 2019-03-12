#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure that when ULDI is on, we link cause downstream modules to get built
when we depend on the component objs.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'uldi'
  test.run_gyp('uldi-depending-on-module.gyp', chdir=CHDIR)
  test.build('uldi-depending-on-module.gyp', 'an_exe', chdir=CHDIR)
  test.built_file_must_exist('a_module.dll', chdir=CHDIR)

  test.pass_test()
