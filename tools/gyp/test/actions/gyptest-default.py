#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simple actions when using the default build target.
"""

import TestGyp

test = TestGyp.TestGyp(workdir='workarea_default')

test.run_gyp('actions.gyp', chdir='src')

test.relocate('src', 'relocate/src')

# Some gyp files use an action that mentions an output but never
# writes it as a means to making the action run on every build.  That
# doesn't mesh well with ninja's semantics.  TODO(evan): figure out
# how to work always-run actions in to ninja.
# Android also can't do this as it doesn't have order-only dependencies.
if test.format in ['ninja', 'android', 'xcode-ninja']:
  test.build('actions.gyp', test.ALL, chdir='relocate/src')
else:
  # Test that an "always run" action increases a counter on multiple
  # invocations, and that a dependent action updates in step.
  test.build('actions.gyp', chdir='relocate/src')
  test.must_match('relocate/src/subdir1/actions-out/action-counter.txt', '1')
  test.must_match('relocate/src/subdir1/actions-out/action-counter_2.txt', '1')
  test.build('actions.gyp', chdir='relocate/src')
  test.must_match('relocate/src/subdir1/actions-out/action-counter.txt', '2')
  test.must_match('relocate/src/subdir1/actions-out/action-counter_2.txt', '2')

  # The "always run" action only counts to 2, but the dependent target
  # will count forever if it's allowed to run. This verifies that the
  # dependent target only runs when the "always run" action generates
  # new output, not just because the "always run" ran.
  test.build('actions.gyp', test.ALL, chdir='relocate/src')
  test.must_match('relocate/src/subdir1/actions-out/action-counter.txt', '2')
  test.must_match('relocate/src/subdir1/actions-out/action-counter_2.txt', '2')

expect = """\
Hello from program.c
Hello from make-prog1.py
Hello from make-prog2.py
"""

if test.format == 'xcode':
  chdir = 'relocate/src/subdir1'
else:
  chdir = 'relocate/src'
test.run_built_executable('program', chdir=chdir, stdout=expect)


test.must_match('relocate/src/subdir2/file.out', "Hello from make-file.py\n")


expect = "Hello from generate_main.py\n"

if test.format == 'xcode':
  chdir = 'relocate/src/subdir3'
else:
  chdir = 'relocate/src'
test.run_built_executable('null_input', chdir=chdir, stdout=expect)


test.pass_test()
