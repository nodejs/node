#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable with C++ defines.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('defines.gyp')

test.build('defines.gyp')

expect = """\
FOO is defined
VALUE is 1
2*PAREN_VALUE is 12
HASH_VALUE is a#1
"""
test.run_built_executable('defines', stdout=expect)

test.pass_test()
