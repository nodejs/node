#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure .s files aren't passed to cl.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'asm-files'
  test.run_gyp('asm-files.gyp', chdir=CHDIR)
  # The compiler will error out if it's passed the .s files, so just make sure
  # the build succeeds. The compiler doesn't directly support building
  # assembler files on Windows, they have to be built explicitly with a
  # third-party tool.
  test.build('asm-files.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
