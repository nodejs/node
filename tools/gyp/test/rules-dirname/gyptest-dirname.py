#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simple rules when using an explicit build target of 'all'.
"""

import TestGyp
import os
import sys

test = TestGyp.TestGyp(formats=['make', 'ninja', 'android', 'xcode', 'msvs'])

test.run_gyp('actions.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('actions.gyp', chdir='relocate/src')

expect = """\
no dir here
hi c
hello baz
"""
if test.format == 'xcode':
  chdir = 'relocate/src/subdir'
else:
  chdir = 'relocate/src'
test.run_built_executable('gencc_int_output', chdir=chdir, stdout=expect)
if test.format == 'msvs':
  test.run_built_executable('gencc_int_output_external', chdir=chdir,
                            stdout=expect)

test.must_match('relocate/src/subdir/foo/bar/baz.dirname',
                os.path.join('foo', 'bar'))
test.must_match('relocate/src/subdir/a/b/c.dirname',
                os.path.join('a', 'b'))

# FIXME the xcode and make generators incorrectly convert RULE_INPUT_PATH
# to an absolute path, making the tests below fail!
if test.format != 'xcode' and test.format != 'make':
  test.must_match('relocate/src/subdir/foo/bar/baz.path',
                  os.path.join('foo', 'bar', 'baz.printvars'))
  test.must_match('relocate/src/subdir/a/b/c.path',
                  os.path.join('a', 'b', 'c.printvars'))

test.pass_test()
