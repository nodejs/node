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
from closure_linter import ecmametadatapass
from closure_linter import javascripttokenizer
from closure_linter import javascripttokens
from closure_linter import requireprovidesorter

# pylint: disable-msg=C6409
TokenType = javascripttokens.JavaScriptTokenType


class RequireProvideSorterTest(googletest.TestCase):
  """Tests for RequireProvideSorter."""

  _tokenizer = javascripttokenizer.JavaScriptTokenizer()
  _metadata_pass = ecmametadatapass.EcmaMetaDataPass()

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
    token = self._tokenizer.TokenizeFile(input_lines)
    self._metadata_pass.Reset()
    self._metadata_pass.Process(token)

    sorter = requireprovidesorter.RequireProvideSorter()
    sorter.FixRequires(token)

    self.assertEquals(expected_lines, self._GetLines(token))

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
