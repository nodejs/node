#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test VCCLCompilerTool EnableEnhancedInstructionSet setting.
"""

import TestGyp

import os
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp()

  CHDIR = 'compiler-flags'
  test.run_gyp('enable-enhanced-instruction-set.gyp', chdir=CHDIR)

  test.build('enable-enhanced-instruction-set.gyp', test.ALL, chdir=CHDIR)

  test.run_built_executable('sse_extensions', chdir=CHDIR,
                            stdout='/arch:SSE\n')
  test.run_built_executable('sse2_extensions', chdir=CHDIR,
                            stdout='/arch:SSE2\n')

  # /arch:AVX introduced in VS2010, but MSBuild support lagged until 2012.
  if os.path.exists(test.built_file_path('avx_extensions')):
    test.run_built_executable('avx_extensions', chdir=CHDIR,
                              stdout='/arch:AVX\n')

  # /arch:IA32 introduced in VS2012.
  if os.path.exists(test.built_file_path('no_extensions')):
    test.run_built_executable('no_extensions', chdir=CHDIR,
                              stdout='/arch:IA32\n')

  # /arch:AVX2 introduced in VS2013r2.
  if os.path.exists(test.built_file_path('avx2_extensions')):
    test.run_built_executable('avx2_extensions', chdir=CHDIR,
                              stdout='/arch:AVX2\n')

  test.pass_test()
