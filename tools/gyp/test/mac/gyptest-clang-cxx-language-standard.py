#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that CLANG_CXX_LANGUAGE_STANDARD works.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['make', 'ninja', 'xcode'])

  test.run_gyp('clang-cxx-language-standard.gyp',
               chdir='clang-cxx-language-standard')

  test.build('clang-cxx-language-standard.gyp', test.ALL,
             chdir='clang-cxx-language-standard')

  test.pass_test()

