#!/usr/bin/env python
# Copyright 2013 The Closure Linter Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Medium tests for the gjslint errorrules.

Currently its just verifying that warnings can't be disabled.
"""



import gflags as flags
import unittest as googletest

from closure_linter import errors
from closure_linter import runner
from closure_linter.common import erroraccumulator

flags.FLAGS.strict = True
flags.FLAGS.limited_doc_files = ('dummy.js', 'externs.js')
flags.FLAGS.closurized_namespaces = ('goog', 'dummy')


class ErrorRulesTest(googletest.TestCase):
  """Test case to for gjslint errorrules."""

  def testNoMaxLineLengthFlagExists(self):
    """Tests that --max_line_length flag does not exists."""
    self.assertTrue('max_line_length' not in flags.FLAGS.FlagDict())

  def testGetMaxLineLength(self):
    """Tests warning are reported for line greater than 80.
    """

    # One line > 100 and one line > 80 and < 100. So should produce two
    # line too long error.
    original = [
        'goog.require(\'dummy.aa\');',
        '',
        'function a() {',
        '  dummy.aa.i = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13'
        ' + 14 + 15 + 16 + 17 + 18 + 19 + 20;',
        '  dummy.aa.j = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13'
        ' + 14 + 15 + 16 + 17 + 18;',
        '}',
        ''
        ]

    # Expect line too long.
    expected = [errors.LINE_TOO_LONG, errors.LINE_TOO_LONG]

    self._AssertErrors(original, expected)

  def testNoDisableFlagExists(self):
    """Tests that --disable flag does not exists."""
    self.assertTrue('disable' not in flags.FLAGS.FlagDict())

  def testWarningsNotDisabled(self):
    """Tests warnings are reported when nothing is disabled.
    """
    original = [
        'goog.require(\'dummy.aa\');',
        'goog.require(\'dummy.Cc\');',
        'goog.require(\'dummy.Dd\');',
        '',
        'function a() {',
        '  dummy.aa.i = 1;',
        '  dummy.Cc.i = 1;',
        '  dummy.Dd.i = 1;',
        '}',
        ]

    expected = [errors.GOOG_REQUIRES_NOT_ALPHABETIZED,
                errors.FILE_MISSING_NEWLINE]

    self._AssertErrors(original, expected)

  def _AssertErrors(self, original, expected_errors, include_header=True):
    """Asserts that the error fixer corrects original to expected."""
    if include_header:
      original = self._GetHeader() + original

    # Trap gjslint's output parse it to get messages added.
    error_accumulator = erroraccumulator.ErrorAccumulator()
    runner.Run('testing.js', error_accumulator, source=original)
    error_nums = [e.code for e in error_accumulator.GetErrors()]

    error_nums.sort()
    expected_errors.sort()
    self.assertListEqual(error_nums, expected_errors)

  def _GetHeader(self):
    """Returns a fake header for a JavaScript file."""
    return [
        '// Copyright 2011 Google Inc. All Rights Reserved.',
        '',
        '/**',
        ' * @fileoverview Fake file overview.',
        ' * @author fake@google.com (Fake Person)',
        ' */',
        ''
        ]


if __name__ == '__main__':
  googletest.main()
