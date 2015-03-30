#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that it's possible for gyp actions to use the result of calling a make
function with "$()".
"""

import TestGyp

test = TestGyp.TestGyp(formats=['android'])

test.run_gyp('make_functions.gyp')

test.build('make_functions.gyp', test.ALL)

file_content = 'A boring test file\n'
test.built_file_must_match('file.in', file_content)
test.built_file_must_match('file.out', file_content)

test.pass_test()
