#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that when the same value is repeated for a gyp define, duplicates are
stripped from the regeneration rule.
"""

import os
import TestGyp

# Regenerating build files when a gyp file changes is currently only supported
# by the make and Android generators.
test = TestGyp.TestGyp(formats=['make', 'android'])

os.environ['GYP_DEFINES'] = 'key=repeated_value key=value1 key=repeated_value'
test.run_gyp('defines.gyp')
test.build('defines.gyp')

# The last occurrence of a repeated set should take precedence over other
# values. See gyptest-multiple-values.py.
test.must_contain('action.txt', 'repeated_value')

# So the regeneration rule needs to use the correct order.
test.must_not_contain(
    'Makefile', '"-Dkey=repeated_value" "-Dkey=value1" "-Dkey=repeated_value"')
test.must_contain('Makefile', '"-Dkey=value1" "-Dkey=repeated_value"')

# Sleep so that the changed gyp file will have a newer timestamp than the
# previously generated build files.
test.sleep()
os.utime("defines.gyp", None)

test.build('defines.gyp')
test.must_contain('action.txt', 'repeated_value')

test.pass_test()
