#!/usr/bin/env python
# Copyright 2011 The Closure Linter Authors. All Rights Reserved.
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


__author__ = 'nnaze@google.com (Nathan Naze)'

import unittest as googletest
from closure_linter.common import tokens


def _CreateDummyToken():
  return tokens.Token('foo', None, 1, 1)


def _CreateDummyTokens(count):
  dummy_tokens = []
  for _ in xrange(count):
    dummy_tokens.append(_CreateDummyToken())
  return dummy_tokens


def _SetTokensAsNeighbors(neighbor_tokens):
  for i in xrange(len(neighbor_tokens)):
    prev_index = i - 1
    next_index = i + 1

    if prev_index >= 0:
      neighbor_tokens[i].previous = neighbor_tokens[prev_index]

    if next_index < len(neighbor_tokens):
      neighbor_tokens[i].next = neighbor_tokens[next_index]


class TokensTest(googletest.TestCase):

  def testIsFirstInLine(self):

    # First token in file (has no previous).
    self.assertTrue(_CreateDummyToken().IsFirstInLine())

    a, b = _CreateDummyTokens(2)
    _SetTokensAsNeighbors([a, b])

    # Tokens on same line
    a.line_number = 30
    b.line_number = 30

    self.assertFalse(b.IsFirstInLine())

    # Tokens on different lines
    b.line_number = 31
    self.assertTrue(b.IsFirstInLine())

  def testIsLastInLine(self):
    # Last token in file (has no next).
    self.assertTrue(_CreateDummyToken().IsLastInLine())

    a, b = _CreateDummyTokens(2)
    _SetTokensAsNeighbors([a, b])

    # Tokens on same line
    a.line_number = 30
    b.line_number = 30
    self.assertFalse(a.IsLastInLine())

    b.line_number = 31
    self.assertTrue(a.IsLastInLine())

  def testIsType(self):
    a = tokens.Token('foo', 'fakeType1', 1, 1)
    self.assertTrue(a.IsType('fakeType1'))
    self.assertFalse(a.IsType('fakeType2'))

  def testIsAnyType(self):
    a = tokens.Token('foo', 'fakeType1', 1, 1)
    self.assertTrue(a.IsAnyType(['fakeType1', 'fakeType2']))
    self.assertFalse(a.IsAnyType(['fakeType3', 'fakeType4']))

  def testRepr(self):
    a = tokens.Token('foo', 'fakeType1', 1, 1)
    self.assertEquals('<Token: fakeType1, "foo", None, 1, None>', str(a))

  def testIter(self):
    dummy_tokens = _CreateDummyTokens(5)
    _SetTokensAsNeighbors(dummy_tokens)
    a, b, c, d, e = dummy_tokens

    i = iter(a)
    self.assertListEqual([a, b, c, d, e], list(i))

  def testReverseIter(self):
    dummy_tokens = _CreateDummyTokens(5)
    _SetTokensAsNeighbors(dummy_tokens)
    a, b, c, d, e = dummy_tokens

    ri = reversed(e)
    self.assertListEqual([e, d, c, b, a], list(ri))


if __name__ == '__main__':
  googletest.main()
