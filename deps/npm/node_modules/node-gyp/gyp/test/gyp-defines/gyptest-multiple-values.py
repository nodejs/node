#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that when multiple values are supplied for a gyp define, the last one
is used.
"""

import os
import TestGyp

test = TestGyp.TestGyp()

os.environ['GYP_DEFINES'] = 'key=value1 key=value2 key=value3'
test.run_gyp('defines.gyp')
test.build('defines.gyp')
test.must_contain('action.txt', 'value3')

# The last occurrence of a repeated set should take precedence over other
# values.
os.environ['GYP_DEFINES'] = 'key=repeated_value key=value1 key=repeated_value'
test.run_gyp('defines.gyp')
if test.format == 'msvs' and not test.uses_msbuild:
  # msvs versions before 2010 don't detect build rule changes not reflected
  # in file system timestamps. Rebuild to see differences.
  test.build('defines.gyp', rebuild=True)
else:
  test.build('defines.gyp')
test.must_contain('action.txt', 'repeated_value')

test.pass_test()
