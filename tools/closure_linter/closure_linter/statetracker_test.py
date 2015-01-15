#!/usr/bin/env python
#
# Copyright 2012 The Closure Linter Authors. All Rights Reserved.
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

"""Unit tests for the statetracker module."""

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

__author__ = ('nnaze@google.com (Nathan Naze)')



import unittest as googletest

from closure_linter import javascripttokens
from closure_linter import statetracker
from closure_linter import testutil


class _FakeDocFlag(object):

  def __repr__(self):
    return '@%s %s' % (self.flag_type, self.name)


class IdentifierTest(googletest.TestCase):

  def testJustIdentifier(self):
    a = javascripttokens.JavaScriptToken(
        'abc', javascripttokens.JavaScriptTokenType.IDENTIFIER, 'abc', 1)

    st = statetracker.StateTracker()
    st.HandleToken(a, None)


class DocCommentTest(googletest.TestCase):

  @staticmethod
  def _MakeDocFlagFake(flag_type, name=None):
    flag = _FakeDocFlag()
    flag.flag_type = flag_type
    flag.name = name
    return flag

  def testDocFlags(self):
    comment = statetracker.DocComment(None)

    a = self._MakeDocFlagFake('param', 'foo')
    comment.AddFlag(a)

    b = self._MakeDocFlagFake('param', '')
    comment.AddFlag(b)

    c = self._MakeDocFlagFake('param', 'bar')
    comment.AddFlag(c)

    self.assertEquals(
        ['foo', 'bar'],
        comment.ordered_params)

    self.assertEquals(
        [a, b, c],
        comment.GetDocFlags())

  def testInvalidate(self):
    comment = statetracker.DocComment(None)

    self.assertFalse(comment.invalidated)
    self.assertFalse(comment.IsInvalidated())

    comment.Invalidate()

    self.assertTrue(comment.invalidated)
    self.assertTrue(comment.IsInvalidated())

  def testSuppressionOnly(self):
    comment = statetracker.DocComment(None)

    self.assertFalse(comment.SuppressionOnly())
    comment.AddFlag(self._MakeDocFlagFake('suppress'))
    self.assertTrue(comment.SuppressionOnly())
    comment.AddFlag(self._MakeDocFlagFake('foo'))
    self.assertFalse(comment.SuppressionOnly())

  def testRepr(self):
    comment = statetracker.DocComment(None)
    comment.AddFlag(self._MakeDocFlagFake('param', 'foo'))
    comment.AddFlag(self._MakeDocFlagFake('param', 'bar'))

    self.assertEquals(
        '<DocComment: [\'foo\', \'bar\'], [@param foo, @param bar]>',
        repr(comment))

  def testDocFlagParam(self):
    comment = self._ParseComment("""
    /**
     * @param {string} [name] Name of customer.
     */""")
    flag = comment.GetFlag('param')
    self.assertEquals('string', flag.type)
    self.assertEquals('string', flag.jstype.ToString())
    self.assertEquals('[name]', flag.name)

  def _ParseComment(self, script):
    """Parse a script that contains one comment and return it."""
    _, comments = testutil.ParseFunctionsAndComments(script)
    self.assertEquals(1, len(comments))
    return comments[0]

if __name__ == '__main__':
  googletest.main()
