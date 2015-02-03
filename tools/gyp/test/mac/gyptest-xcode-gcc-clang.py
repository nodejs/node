#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that xcode-style GCC_... settings that require clang are handled
properly.
"""

import TestGyp

import os
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'xcode-gcc'
  test.run_gyp('test-clang.gyp', chdir=CHDIR)

  test.build('test-clang.gyp', 'aliasing_yes', chdir=CHDIR)
  test.run_built_executable('aliasing_yes', chdir=CHDIR, stdout="1\n")
  test.build('test-clang.gyp', 'aliasing_no', chdir=CHDIR)
  test.run_built_executable('aliasing_no', chdir=CHDIR, stdout="0\n")

  # The default behavior changed: strict aliasing used to be off, now it's on
  # by default. The important part is that this is identical for all generators
  # (which it is). TODO(thakis): Enable this once the bots have a newer Xcode.
  #test.build('test-clang.gyp', 'aliasing_default', chdir=CHDIR)
  #test.run_built_executable('aliasing_default', chdir=CHDIR, stdout="1\n")
  # For now, just check the generated ninja file:
  if test.format == 'ninja':
    contents = open(test.built_file_path('obj/aliasing_default.ninja',
                                         chdir=CHDIR)).read()
    if 'strict-aliasing' in contents:
      test.fail_test()

  test.pass_test()
