#!/usr/bin/env python
#
# Copyright 2008 The Closure Linter Authors. All Rights Reserved.
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

"""Unit tests for the runner module."""

__author__ = ('nnaze@google.com (Nathan Naze)')

import StringIO


import mox


import unittest as googletest

from closure_linter import errors
from closure_linter import runner
from closure_linter.common import error
from closure_linter.common import errorhandler
from closure_linter.common import tokens


class LimitedDocTest(googletest.TestCase):

  def testIsLimitedDocCheck(self):
    self.assertTrue(runner._IsLimitedDocCheck('foo_test.js', ['_test.js']))
    self.assertFalse(runner._IsLimitedDocCheck('foo_bar.js', ['_test.js']))

    self.assertTrue(runner._IsLimitedDocCheck(
        'foo_moo.js', ['moo.js', 'quack.js']))
    self.assertFalse(runner._IsLimitedDocCheck(
        'foo_moo.js', ['woof.js', 'quack.js']))


class RunnerTest(googletest.TestCase):

  def setUp(self):
    self.mox = mox.Mox()

  def testRunOnMissingFile(self):
    mock_error_handler = self.mox.CreateMock(errorhandler.ErrorHandler)

    def ValidateError(err):
      return (isinstance(err, error.Error) and
              err.code is errors.FILE_NOT_FOUND and
              err.token is None)

    mock_error_handler.HandleFile('does_not_exist.js', None)
    mock_error_handler.HandleError(mox.Func(ValidateError))
    mock_error_handler.FinishFile()

    self.mox.ReplayAll()

    runner.Run('does_not_exist.js', mock_error_handler)

    self.mox.VerifyAll()

  def testBadTokenization(self):
    mock_error_handler = self.mox.CreateMock(errorhandler.ErrorHandler)

    def ValidateError(err):
      return (isinstance(err, error.Error) and
              err.code is errors.FILE_IN_BLOCK and
              err.token.string == '}')

    mock_error_handler.HandleFile('foo.js', mox.IsA(tokens.Token))
    mock_error_handler.HandleError(mox.Func(ValidateError))
    mock_error_handler.HandleError(mox.IsA(error.Error))
    mock_error_handler.FinishFile()

    self.mox.ReplayAll()

    source = StringIO.StringIO(_BAD_TOKENIZATION_SCRIPT)
    runner.Run('foo.js', mock_error_handler, source)

    self.mox.VerifyAll()


_BAD_TOKENIZATION_SCRIPT = """
function foo () {
  var a = 3;
  var b = 2;
  return b + a; /* Comment not closed
}
"""


if __name__ == '__main__':
  googletest.main()
