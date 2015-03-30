#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies --root-target removes the unnecessary targets.
"""

import TestGyp

test = TestGyp.TestGyp()
# The xcode-ninja generator has its own logic for which targets to include
if test.format == 'xcode-ninja':
  test.skip_test()

build_error_code = {
  'android': 2,
  'cmake': 1,
  'make': 2,
  'msvs': 1,
  'ninja': 1,
  'xcode': 65,
}[test.format]

# By default, everything will be included.
test.run_gyp('test1.gyp')
test.build('test2.gyp', 'lib1')
test.build('test2.gyp', 'lib2')
test.build('test2.gyp', 'lib3')
test.build('test2.gyp', 'lib_indirect')
test.build('test1.gyp', 'program1')
test.build('test1.gyp', 'program2')
test.build('test1.gyp', 'program3')

# With deep dependencies of program1 only.
test.run_gyp('test1.gyp', '--root-target=program1')
test.build('test2.gyp', 'lib1')
test.build('test2.gyp', 'lib2', status=build_error_code, stderr=None)
test.build('test2.gyp', 'lib3', status=build_error_code, stderr=None)
test.build('test2.gyp', 'lib_indirect')
test.build('test1.gyp', 'program1')
test.build('test1.gyp', 'program2', status=build_error_code, stderr=None)
test.build('test1.gyp', 'program3', status=build_error_code, stderr=None)

# With deep dependencies of program2 only.
test.run_gyp('test1.gyp', '--root-target=program2')
test.build('test2.gyp', 'lib1', status=build_error_code, stderr=None)
test.build('test2.gyp', 'lib2')
test.build('test2.gyp', 'lib3', status=build_error_code, stderr=None)
test.build('test2.gyp', 'lib_indirect')
test.build('test1.gyp', 'program1', status=build_error_code, stderr=None)
test.build('test1.gyp', 'program2')
test.build('test1.gyp', 'program3', status=build_error_code, stderr=None)

# With deep dependencies of program1 and program2.
test.run_gyp('test1.gyp', '--root-target=program1', '--root-target=program2')
test.build('test2.gyp', 'lib1')
test.build('test2.gyp', 'lib2')
test.build('test2.gyp', 'lib3', status=build_error_code, stderr=None)
test.build('test2.gyp', 'lib_indirect')
test.build('test1.gyp', 'program1')
test.build('test1.gyp', 'program2')
test.build('test1.gyp', 'program3', status=build_error_code, stderr=None)

test.pass_test()
