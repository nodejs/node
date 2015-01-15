#!/usr/bin/env python
#
# Copyright 2012 The Closure Linter Authors. All Rights Reserved.
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

"""Unit tests for RequireProvideSorter."""



import unittest as googletest
from closure_linter import javascripttokens
from closure_linter import requireprovidesorter
from closure_linter import testutil

# pylint: disable=g-bad-name
TokenType = javascripttokens.JavaScriptTokenType


class RequireProvideSorterTest(googletest.TestCase):
  """Tests for RequireProvideSorter."""

  def testGetFixedProvideString(self):
    """Tests that fixed string constains proper comments also."""
    input_lines = [
        'goog.provide(\'package.xyz\');',
        '/** @suppress {extraprovide} **/',
        'goog.provide(\'package.abcd\');'
    ]

    expected_lines = [
        '/** @suppress {extraprovide} **/',
        'goog.provide(\'package.abcd\');',
        'goog.provide(\'package.xyz\');'
    ]

    token = testutil.TokenizeSourceAndRunEcmaPass(input_lines)

    sorter = requireprovidesorter.RequireProvideSorter()
    fixed_provide_string = sorter.GetFixedProvideString(token)

    self.assertEquals(expected_lines, fixed_provide_string.splitlines())

  def testGetFixedRequireString(self):
    """Tests that fixed string constains proper comments also."""
    input_lines = [
        'goog.require(\'package.xyz\');',
        '/** This is needed for scope. **/',
        'goog.require(\'package.abcd\');'
    ]

    expected_lines = [
        '/** This is needed for scope. **/',
        'goog.require(\'package.abcd\');',
        'goog.require(\'package.xyz\');'
    ]

    token = testutil.TokenizeSourceAndRunEcmaPass(input_lines)

    sorter = requireprovidesorter.RequireProvideSorter()
    fixed_require_string = sorter.GetFixedRequireString(token)

    self.assertEquals(expected_lines, fixed_require_string.splitlines())

  def testFixRequires_removeBlankLines(self):
    """Tests that blank lines are omitted in sorted goog.require statements."""
    input_lines = [
        'goog.provide(\'package.subpackage.Whatever\');',
        '',
        'goog.require(\'package.subpackage.ClassB\');',
        '',
        'goog.require(\'package.subpackage.ClassA\');'
    ]
    expected_lines = [
        'goog.provide(\'package.subpackage.Whatever\');',
        '',
        'goog.require(\'package.subpackage.ClassA\');',
        'goog.require(\'package.subpackage.ClassB\');'
    ]
    token = testutil.TokenizeSourceAndRunEcmaPass(input_lines)

    sorter = requireprovidesorter.RequireProvideSorter()
    sorter.FixRequires(token)

    self.assertEquals(expected_lines, self._GetLines(token))

  def fixRequiresTest_withTestOnly(self, position):
    """Regression-tests sorting even with a goog.setTestOnly statement.

    Args:
      position: The position in the list where to insert the goog.setTestOnly
                statement. Will be used to test all possible combinations for
                this test.
    """
    input_lines = [
        'goog.provide(\'package.subpackage.Whatever\');',
        '',
        'goog.require(\'package.subpackage.ClassB\');',
        'goog.require(\'package.subpackage.ClassA\');'
    ]
    expected_lines = [
        'goog.provide(\'package.subpackage.Whatever\');',
        '',
        'goog.require(\'package.subpackage.ClassA\');',
        'goog.require(\'package.subpackage.ClassB\');'
    ]
    input_lines.insert(position, 'goog.setTestOnly();')
    expected_lines.insert(position, 'goog.setTestOnly();')

    token = testutil.TokenizeSourceAndRunEcmaPass(input_lines)

    sorter = requireprovidesorter.RequireProvideSorter()
    sorter.FixRequires(token)

    self.assertEquals(expected_lines, self._GetLines(token))

  def testFixRequires_withTestOnly(self):
    """Regression-tests sorting even after a goog.setTestOnly statement."""

    # goog.setTestOnly at first line.
    self.fixRequiresTest_withTestOnly(position=0)

    # goog.setTestOnly after goog.provide.
    self.fixRequiresTest_withTestOnly(position=1)

    # goog.setTestOnly before goog.require.
    self.fixRequiresTest_withTestOnly(position=2)

    # goog.setTestOnly after goog.require.
    self.fixRequiresTest_withTestOnly(position=4)

  def _GetLines(self, token):
    """Returns an array of lines based on the specified token stream."""
    lines = []
    line = ''
    while token:
      line += token.string
      if token.IsLastInLine():
        lines.append(line)
        line = ''
      token = token.next
    return lines

if __name__ == '__main__':
  googletest.main()
