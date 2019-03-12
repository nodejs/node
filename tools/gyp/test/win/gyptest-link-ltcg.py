#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure LTCG is working properly.
"""

import TestGyp

import sys

if not sys.platform == 'win32':
  print('Only for Windows')
  sys.exit(2)

test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

CHDIR = 'linker-flags'
test.run_gyp('ltcg.gyp', chdir=CHDIR)

# Here we expect LTCG is able to inline functions beyond compile unit.
# Note: This marker is embedded in 'inline_test_main.cc'
INLINE_MARKER = '==== inlined ===='

# link.exe generates following lines when LTCG is enabled.
# Note: Future link.exe may or may not generate them. Update as needed.
LTCG_LINKER_MESSAGES = ['Generating code', 'Finished generating code']

# test 'LinkTimeCodeGenerationOptionDefault'
test.build('ltcg.gyp', 'test_ltcg_off', chdir=CHDIR)
test.run_built_executable('test_ltcg_off', chdir=CHDIR)
test.must_not_contain_any_line(test.stdout(), [INLINE_MARKER])

# test 'LinkTimeCodeGenerationOptionUse'
test.build('ltcg.gyp', 'test_ltcg_on', chdir=CHDIR)
if test.format == 'ninja':
  # Make sure ninja win_tool.py filters out noisy lines.
  test.must_not_contain_any_line(test.stdout(), LTCG_LINKER_MESSAGES)
elif test.format == 'msvs':
  test.must_contain_any_line(test.stdout(), LTCG_LINKER_MESSAGES)
test.run_built_executable('test_ltcg_on', chdir=CHDIR)
test.must_contain_any_line(test.stdout(), [INLINE_MARKER])

test.pass_test()
