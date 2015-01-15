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

"""Tools to match goog.scope alias statements."""

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

__author__ = ('nnaze@google.com (Nathan Naze)')

import itertools

from closure_linter import ecmametadatapass
from closure_linter import tokenutil
from closure_linter.javascripttokens import JavaScriptTokenType



def IsGoogScopeBlock(context):
  """Whether the given context is a goog.scope block.

  This function only checks that the block is a function block inside
  a goog.scope() call.

  TODO(nnaze): Implement goog.scope checks that verify the call is
  in the root context and contains only a single function literal.

  Args:
    context: An EcmaContext of type block.

  Returns:
    Whether the context is a goog.scope block.
  """

  if context.type != ecmametadatapass.EcmaContext.BLOCK:
    return False

  if not _IsFunctionLiteralBlock(context):
    return False

  # Check that this function is contained by a group
  # of form "goog.scope(...)".
  parent = context.parent
  if parent and parent.type is ecmametadatapass.EcmaContext.GROUP:

    last_code_token = parent.start_token.metadata.last_code

    if (last_code_token and
        last_code_token.type is JavaScriptTokenType.IDENTIFIER and
        last_code_token.string == 'goog.scope'):
      return True

  return False


def _IsFunctionLiteralBlock(block_context):
  """Check if a context is a function literal block (without parameters).

  Example function literal block: 'function() {}'

  Args:
    block_context: An EcmaContext of type block.

  Returns:
    Whether this context is a function literal block.
  """

  previous_code_tokens_iter = itertools.ifilter(
      lambda token: token not in JavaScriptTokenType.NON_CODE_TYPES,
      reversed(block_context.start_token))

  # Ignore the current token
  next(previous_code_tokens_iter, None)

  # Grab the previous three tokens and put them in correct order.
  previous_code_tokens = list(itertools.islice(previous_code_tokens_iter, 3))
  previous_code_tokens.reverse()

  # There aren't three previous tokens.
  if len(previous_code_tokens) is not 3:
    return False

  # Check that the previous three code tokens are "function ()"
  previous_code_token_types = [token.type for token in previous_code_tokens]
  if (previous_code_token_types == [
      JavaScriptTokenType.FUNCTION_DECLARATION,
      JavaScriptTokenType.START_PARAMETERS,
      JavaScriptTokenType.END_PARAMETERS]):
    return True

  return False


def IsInClosurizedNamespace(symbol, closurized_namespaces):
  """Match a goog.scope alias.

  Args:
    symbol: An identifier like 'goog.events.Event'.
    closurized_namespaces: Iterable of valid Closurized namespaces (strings).

  Returns:
    True if symbol is an identifier in a Closurized namespace, otherwise False.
  """
  for ns in closurized_namespaces:
    if symbol.startswith(ns + '.'):
      return True

  return False


def _GetVarAssignmentTokens(context):
  """Returns the tokens from context if it is a var assignment.

  Args:
    context: An EcmaContext.

  Returns:
    If a var assignment, the tokens contained within it w/o the trailing
    semicolon.
  """
  if context.type != ecmametadatapass.EcmaContext.VAR:
    return

  # Get the tokens in this statement.
  if context.start_token and context.end_token:
    statement_tokens = tokenutil.GetTokenRange(context.start_token,
                                               context.end_token)
  else:
    return

  # And now just those tokens that are actually code.
  is_non_code_type = lambda t: t.type not in JavaScriptTokenType.NON_CODE_TYPES
  code_tokens = filter(is_non_code_type, statement_tokens)

  # Pop off the semicolon if present.
  if code_tokens and code_tokens[-1].IsType(JavaScriptTokenType.SEMICOLON):
    code_tokens.pop()

  if len(code_tokens) < 4:
    return

  if (code_tokens[0].IsKeyword('var') and
      code_tokens[1].IsType(JavaScriptTokenType.SIMPLE_LVALUE) and
      code_tokens[2].IsOperator('=')):
    return code_tokens


def MatchAlias(context):
  """Match an alias statement (some identifier assigned to a variable).

  Example alias: var MyClass = proj.longNamespace.MyClass.

  Args:
    context: An EcmaContext of type EcmaContext.VAR.

  Returns:
    If a valid alias, returns a tuple of alias and symbol, otherwise None.
  """
  code_tokens = _GetVarAssignmentTokens(context)
  if code_tokens is None:
    return

  if all(tokenutil.IsIdentifierOrDot(t) for t in code_tokens[3:]):
    # var Foo = bar.Foo;
    alias, symbol = code_tokens[1], code_tokens[3]
    # Mark both tokens as an alias definition to not count them as usages.
    alias.metadata.is_alias_definition = True
    symbol.metadata.is_alias_definition = True
    return alias.string, tokenutil.GetIdentifierForToken(symbol)


def MatchModuleAlias(context):
  """Match an alias statement in a goog.module style import.

  Example alias: var MyClass = goog.require('proj.longNamespace.MyClass').

  Args:
    context: An EcmaContext.

  Returns:
    If a valid alias, returns a tuple of alias and symbol, otherwise None.
  """
  code_tokens = _GetVarAssignmentTokens(context)
  if code_tokens is None:
    return

  if(code_tokens[3].IsType(JavaScriptTokenType.IDENTIFIER) and
     code_tokens[3].string == 'goog.require'):
    # var Foo = goog.require('bar.Foo');
    alias = code_tokens[1]
    symbol = tokenutil.GetStringAfterToken(code_tokens[3])
    if symbol:
      alias.metadata.is_alias_definition = True
      return alias.string, symbol
