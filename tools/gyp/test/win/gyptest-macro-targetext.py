#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure macro expansion of $(TargetExt) is handled.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'vs-macros'
  test.run_gyp('targetext.gyp', chdir=CHDIR)
  test.build('targetext.gyp', test.ALL, chdir=CHDIR)
  test.built_file_must_exist('executable.exe', chdir=CHDIR)
  test.built_file_must_exist('loadable_module.dll', chdir=CHDIR)
  test.built_file_must_exist('shared_library.dll', chdir=CHDIR)
  test.built_file_must_exist('static_library.lib', chdir=CHDIR)
  test.built_file_must_exist('product_extension.library', chdir=CHDIR)
  test.pass_test()
