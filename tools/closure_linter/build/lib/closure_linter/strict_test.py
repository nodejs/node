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

"""Tests for gjslint --strict.

Tests errors that can be thrown by gjslint when in strict mode.
"""



import unittest

import gflags as flags
import unittest as googletest

from closure_linter import errors
from closure_linter import runner
from closure_linter.common import erroraccumulator

flags.FLAGS.strict = True


class StrictTest(unittest.TestCase):
  """Tests scenarios where strict generates warnings."""

  def testUnclosedString(self):
    """Tests warnings are reported when nothing is disabled.

       b/11450054.
    """
    original = [
        'bug = function() {',
        '  (\'foo\'\');',
        '};',
        '',
        ]

    expected = [errors.FILE_DOES_NOT_PARSE, errors.MULTI_LINE_STRING,
                errors.FILE_IN_BLOCK]
    self._AssertErrors(original, expected)

  def _AssertErrors(self, original, expected_errors):
    """Asserts that the error fixer corrects original to expected."""

    # Trap gjslint's output parse it to get messages added.
    error_accumulator = erroraccumulator.ErrorAccumulator()
    runner.Run('testing.js', error_accumulator, source=original)
    error_nums = [e.code for e in error_accumulator.GetErrors()]

    error_nums.sort()
    expected_errors.sort()
    self.assertListEqual(error_nums, expected_errors)

if __name__ == '__main__':
  googletest.main()

