#!/usr/bin/env python
# Copyright 2008 The Closure Linter Authors. All Rights Reserved.
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

"""Parser for JavaScript files."""



from closure_linter import javascripttokens
from closure_linter import statetracker
from closure_linter import tokenutil

# Shorthand
Type = javascripttokens.JavaScriptTokenType


class JsDocFlag(statetracker.DocFlag):
  """Javascript doc flag object.

  Attribute:
    flag_type: param, return, define, type, etc.
    flag_token: The flag token.
    type_start_token: The first token specifying the flag JS type,
      including braces.
    type_end_token: The last token specifying the flag JS type,
      including braces.
    type: The type spec string.
    jstype: The type spec, a TypeAnnotation instance.
    name_token: The token specifying the flag name.
    name: The flag name
    description_start_token: The first token in the description.
    description_end_token: The end token in the description.
    description: The description.
  """

  # Please keep these lists alphabetized.

  # Some projects use the following extensions to JsDoc.
  # TODO(robbyw): determine which of these, if any, should be illegal.
  EXTENDED_DOC = frozenset([
      'class', 'code', 'desc', 'final', 'hidden', 'inheritDoc', 'link',
      'meaning', 'provideGoog', 'throws'])

  LEGAL_DOC = EXTENDED_DOC | statetracker.DocFlag.LEGAL_DOC


class JavaScriptStateTracker(statetracker.StateTracker):
  """JavaScript state tracker.

  Inherits from the core EcmaScript StateTracker adding extra state tracking
  functionality needed for JavaScript.
  """

  def __init__(self):
    """Initializes a JavaScript token stream state tracker."""
    statetracker.StateTracker.__init__(self, JsDocFlag)

  def Reset(self):
    self._scope_depth = 0
    self._block_stack = []
    super(JavaScriptStateTracker, self).Reset()

  def InTopLevel(self):
    """Compute whether we are at the top level in the class.

    This function call is language specific.  In some languages like
    JavaScript, a function is top level if it is not inside any parenthesis.
    In languages such as ActionScript, a function is top level if it is directly
    within a class.

    Returns:
      Whether we are at the top level in the class.
    """
    return self._scope_depth == self.ParenthesesDepth()

  def InFunction(self):
    """Returns true if the current token is within a function.

    This js-specific override ignores goog.scope functions.

    Returns:
      True if the current token is within a function.
    """
    return self._scope_depth != self.FunctionDepth()

  def InNonScopeBlock(self):
    """Compute whether we are nested within a non-goog.scope block.

    Returns:
      True if the token is not enclosed in a block that does not originate from
      a goog.scope statement. False otherwise.
    """
    return self._scope_depth != self.BlockDepth()

  def GetBlockType(self, token):
    """Determine the block type given a START_BLOCK token.

    Code blocks come after parameters, keywords  like else, and closing parens.

    Args:
      token: The current token. Can be assumed to be type START_BLOCK
    Returns:
      Code block type for current token.
    """
    last_code = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES, reverse=True)
    if last_code.type in (Type.END_PARAMETERS, Type.END_PAREN,
                          Type.KEYWORD) and not last_code.IsKeyword('return'):
      return self.CODE
    else:
      return self.OBJECT_LITERAL

  def GetCurrentBlockStart(self):
    """Gets the start token of current block.

    Returns:
      Starting token of current block. None if not in block.
    """
    if self._block_stack:
      return self._block_stack[-1]
    else:
      return None

  def HandleToken(self, token, last_non_space_token):
    """Handles the given token and updates state.

    Args:
      token: The token to handle.
      last_non_space_token: The last non space token encountered
    """
    if token.type == Type.START_BLOCK:
      self._block_stack.append(token)
    if token.type == Type.IDENTIFIER and token.string == 'goog.scope':
      self._scope_depth += 1
    if token.type == Type.END_BLOCK:
      start_token = self._block_stack.pop()
      if tokenutil.GoogScopeOrNoneFromStartBlock(start_token):
        self._scope_depth -= 1
    super(JavaScriptStateTracker, self).HandleToken(token,
                                                    last_non_space_token)
