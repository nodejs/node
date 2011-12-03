#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies two actions can be attached to the same input files.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('actions.gyp', chdir='src')

test.relocate('src', 'relocate/src')

# Test that two actions can be attached to the same inputs.
test.build('actions.gyp', test.ALL, chdir='relocate/src')
test.must_contain('relocate/src/output1.txt', 'hello there')
test.must_contain('relocate/src/output2.txt', 'hello there')
test.must_contain('relocate/src/output3.txt', 'hello there')
test.must_contain('relocate/src/output4.txt', 'hello there')

# Test that process_outputs_as_sources works in conjuction with merged
# actions.
test.run_built_executable(
    'multiple_action_source_filter',
    chdir='relocate/src',
    stdout=(
        '{\n'
        'bar\n'
        'car\n'
        'dar\n'
        'ear\n'
        '}\n'
    ),
)


test.pass_test()
