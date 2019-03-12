#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Verifies that stdout and stderr from rules get logged in the build's
stdout."""

import sys
import TestGyp

if not sys.platform == 'win32':
  exit(2)

test = TestGyp.TestGyp(formats=['msvs'])

test.run_gyp('rules-stdout-stderr.gyp')
test.must_contain('test.vcxproj', 'test.targets')

expected_stdout_lines = [
  'testing stdout',
  'This will go to stdout',

  # Note: stderr output from rules will go to the build's stdout.
  'testing stderr',
  'This will go to stderr',
]
test.build('rules-stdout-stderr.gyp', test.ALL)
test.must_contain_all_lines(test.stdout(), expected_stdout_lines)

test.pass_test()
