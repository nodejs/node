#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure LTCG is working properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('ltcg.gyp', chdir=CHDIR)

  # Here we expect LTCG is able to inline functions beyond compile unit.
  # Note: This marker is embedded in 'inline_test_main.cc'
  INLINE_MARKER = '==== inlined ===='

  # test 'LinkTimeCodeGenerationOptionDefault'
  test.build('ltcg.gyp', 'test_ltcg_off', chdir=CHDIR)
  test.run_built_executable('test_ltcg_off', chdir=CHDIR)
  test.must_not_contain_any_line(test.stdout(), [INLINE_MARKER])

  # test 'LinkTimeCodeGenerationOptionUse'
  test.build('ltcg.gyp', 'test_ltcg_on', chdir=CHDIR)
  test.must_contain_any_line(test.stdout(), ['Generating code'])
  test.run_built_executable('test_ltcg_on', chdir=CHDIR)
  test.must_contain_any_line(test.stdout(), [INLINE_MARKER])

  test.pass_test()
