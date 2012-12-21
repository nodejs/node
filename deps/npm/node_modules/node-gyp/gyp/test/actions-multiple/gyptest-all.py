#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies two actions can be attached to the same input files.
"""

import sys

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('actions.gyp', chdir='src')

test.relocate('src', 'relocate/src')

# Test of fine-grained dependencies for generators that can build individual
# files on demand.
# In particular:
#   - TargetA depends on TargetB.
#   - TargetA and TargetB are 'none' type with actions attached.
#   - TargetA has multiple actions.
#   - An output from one of the actions in TargetA (not the first listed),
#     is requested as the build target.
# Ensure that TargetB gets built.
#
# This sub-test can only be done with generators/build tools that can
# be asked to build individual files rather than whole targets (make, ninja).
if test.format in ['make', 'ninja']:
  # Select location of target based on generator.
  if test.format == 'make':
    target = 'multi2.txt'
  elif test.format == 'ninja':
    if sys.platform in ['win32', 'cygwin']:
      target = '..\\..\\multi2.txt'
    else:
      target = '../../multi2.txt'
  else:
    assert False
  test.build('actions.gyp', chdir='relocate/src', target=target)
  test.must_contain('relocate/src/multi2.txt', 'hello there')
  test.must_contain('relocate/src/multi_dep.txt', 'hello there')


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
