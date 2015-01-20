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

"""Unit tests for the scopeutil module."""

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

__author__ = ('nnaze@google.com (Nathan Naze)')


import unittest as googletest

from closure_linter import ecmametadatapass
from closure_linter import scopeutil
from closure_linter import testutil


def _FindContexts(start_token):
  """Depth first search of all contexts referenced by a token stream.

  Includes contexts' parents, which might not be directly referenced
  by any token in the stream.

  Args:
    start_token: First token in the token stream.

  Yields:
    All contexts referenced by this token stream.
  """

  seen_contexts = set()

  # For each token, yield the context if we haven't seen it before.
  for token in start_token:

    token_context = token.metadata.context
    contexts = [token_context]

    # Also grab all the context's ancestors.
    parent = token_context.parent
    while parent:
      contexts.append(parent)
      parent = parent.parent

    # Yield each of these contexts if we've not seen them.
    for context in contexts:
      if context not in seen_contexts:
        yield context

      seen_contexts.add(context)


def _FindFirstContextOfType(token, context_type):
  """Returns the first statement context."""
  for context in _FindContexts(token):
    if context.type == context_type:
      return context


def _ParseAssignment(script):
  start_token = testutil.TokenizeSourceAndRunEcmaPass(script)
  statement = _FindFirstContextOfType(
      start_token, ecmametadatapass.EcmaContext.VAR)
  return statement


class StatementTest(googletest.TestCase):

  def assertAlias(self, expected_match, script):
    statement = _ParseAssignment(script)
    match = scopeutil.MatchAlias(statement)
    self.assertEquals(expected_match, match)

  def assertModuleAlias(self, expected_match, script):
    statement = _ParseAssignment(script)
    match = scopeutil.MatchModuleAlias(statement)
    self.assertEquals(expected_match, match)

  def testSimpleAliases(self):
    self.assertAlias(
        ('foo', 'goog.foo'),
        'var foo = goog.foo;')

    self.assertAlias(
        ('foo', 'goog.foo'),
        'var foo = goog.foo')  # No semicolon

  def testAliasWithComment(self):
    self.assertAlias(
        ('Component', 'goog.ui.Component'),
        'var Component = /* comment */ goog.ui.Component;')

  def testMultilineAlias(self):
    self.assertAlias(
        ('Component', 'goog.ui.Component'),
        'var Component = \n  goog.ui.\n  Component;')

  def testNonSymbolAliasVarStatements(self):
    self.assertAlias(None, 'var foo = 3;')
    self.assertAlias(None, 'var foo = function() {};')
    self.assertAlias(None, 'var foo = bar ? baz : qux;')

  def testModuleAlias(self):
    self.assertModuleAlias(
        ('foo', 'goog.foo'),
        'var foo = goog.require("goog.foo");')
    self.assertModuleAlias(
        None,
        'var foo = goog.require(notastring);')


class ScopeBlockTest(googletest.TestCase):

  @staticmethod
  def _GetBlocks(source):
    start_token = testutil.TokenizeSourceAndRunEcmaPass(source)
    for context in _FindContexts(start_token):
      if context.type is ecmametadatapass.EcmaContext.BLOCK:
        yield context

  def assertNoBlocks(self, script):
    blocks = list(self._GetBlocks(script))
    self.assertEquals([], blocks)

  def testNotBlocks(self):
    # Ensure these are not considered blocks.
    self.assertNoBlocks('goog.scope(if{});')
    self.assertNoBlocks('goog.scope(for{});')
    self.assertNoBlocks('goog.scope(switch{});')
    self.assertNoBlocks('goog.scope(function foo{});')

  def testNonScopeBlocks(self):

    blocks = list(self._GetBlocks('goog.scope(try{});'))
    self.assertEquals(1, len(blocks))
    self.assertFalse(scopeutil.IsGoogScopeBlock(blocks.pop()))

    blocks = list(self._GetBlocks('goog.scope(function(a,b){});'))
    self.assertEquals(1, len(blocks))
    self.assertFalse(scopeutil.IsGoogScopeBlock(blocks.pop()))

    blocks = list(self._GetBlocks('goog.scope(try{} catch(){});'))
    # Two blocks: try and catch.
    self.assertEquals(2, len(blocks))
    self.assertFalse(scopeutil.IsGoogScopeBlock(blocks.pop()))
    self.assertFalse(scopeutil.IsGoogScopeBlock(blocks.pop()))

    blocks = list(self._GetBlocks('goog.scope(try{} catch(){} finally {});'))
    self.assertEquals(3, len(blocks))
    self.assertFalse(scopeutil.IsGoogScopeBlock(blocks.pop()))
    self.assertFalse(scopeutil.IsGoogScopeBlock(blocks.pop()))
    self.assertFalse(scopeutil.IsGoogScopeBlock(blocks.pop()))


class AliasTest(googletest.TestCase):

  def setUp(self):
    self.start_token = testutil.TokenizeSourceAndRunEcmaPass(_TEST_SCRIPT)

  def testMatchAliasStatement(self):
    matches = set()
    for context in _FindContexts(self.start_token):
      match = scopeutil.MatchAlias(context)
      if match:
        matches.add(match)

    self.assertEquals(
        set([('bar', 'baz'),
             ('foo', 'this.foo_'),
             ('Component', 'goog.ui.Component'),
             ('MyClass', 'myproject.foo.MyClass'),
             ('NonClosurizedClass', 'aaa.bbb.NonClosurizedClass')]),
        matches)

  def testMatchAliasStatement_withClosurizedNamespaces(self):

    closurized_namepaces = frozenset(['goog', 'myproject'])

    matches = set()
    for context in _FindContexts(self.start_token):
      match = scopeutil.MatchAlias(context)
      if match:
        unused_alias, symbol = match
        if scopeutil.IsInClosurizedNamespace(symbol, closurized_namepaces):
          matches.add(match)

    self.assertEquals(
        set([('MyClass', 'myproject.foo.MyClass'),
             ('Component', 'goog.ui.Component')]),
        matches)

_TEST_SCRIPT = """
goog.scope(function() {
  var Component = goog.ui.Component; // scope alias
  var MyClass = myproject.foo.MyClass; // scope alias

  // Scope alias of non-Closurized namespace.
  var NonClosurizedClass = aaa.bbb.NonClosurizedClass;

  var foo = this.foo_; // non-scope object property alias
  var bar = baz; // variable alias

  var component = new Component();
});

"""

if __name__ == '__main__':
  googletest.main()
