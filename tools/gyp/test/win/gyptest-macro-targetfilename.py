#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure macro expansion of $(TargetFileName) is handled.
"""

from __future__ import print_function

import TestGyp

import os
import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])
  if not (test.format == 'msvs' and
          int(os.environ.get('GYP_MSVS_VERSION', 0)) == 2013):
    CHDIR = 'vs-macros'
    test.run_gyp('targetfilename.gyp', chdir=CHDIR)
    test.build('targetfilename.gyp', test.ALL, chdir=CHDIR)
    test.built_file_must_exist('test_targetfilename_executable.exe', chdir=CHDIR)
    test.built_file_must_exist('test_targetfilename_loadable_module.dll',
                              chdir=CHDIR)
    test.built_file_must_exist('test_targetfilename_shared_library.dll',
                              chdir=CHDIR)
    test.built_file_must_exist('test_targetfilename_static_library.lib',
                              chdir=CHDIR)
    test.built_file_must_exist('test_targetfilename_product_extension.foo',
                              chdir=CHDIR)
    test.pass_test()
