#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure linker OutputFile setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('outputfile.gyp', chdir=CHDIR)
  test.build('outputfile.gyp', test.ALL, chdir=CHDIR)

  test.built_file_must_exist('blorp.exe', chdir=CHDIR)
  test.built_file_must_exist('blorp.dll', chdir=CHDIR)
  test.built_file_must_exist('subdir/blorp.exe', chdir=CHDIR)
  test.built_file_must_exist('blorp.lib', chdir=CHDIR)
  test.built_file_must_exist('subdir/blorp.lib', chdir=CHDIR)

  test.pass_test()
