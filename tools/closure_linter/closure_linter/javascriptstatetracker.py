#!/usr/bin/env python
#
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
    type: The JavaScript type spec.
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
      'meaning', 'protected', 'notypecheck', 'throws'])

  LEGAL_DOC = EXTENDED_DOC | statetracker.DocFlag.LEGAL_DOC

  def __init__(self, flag_token):
    """Creates the JsDocFlag object and attaches it to the given start token.

    Args:
      flag_token: The starting token of the flag.
    """
    statetracker.DocFlag.__init__(self, flag_token)


class JavaScriptStateTracker(statetracker.StateTracker):
  """JavaScript state tracker.

  Inherits from the core EcmaScript StateTracker adding extra state tracking
  functionality needed for JavaScript.
  """

  def __init__(self):
    """Initializes a JavaScript token stream state tracker."""
    statetracker.StateTracker.__init__(self, JsDocFlag)

  def InTopLevel(self):
    """Compute whether we are at the top level in the class.

    This function call is language specific.  In some languages like
    JavaScript, a function is top level if it is not inside any parenthesis.
    In languages such as ActionScript, a function is top level if it is directly
    within a class.

    Returns:
      Whether we are at the top level in the class.
    """
    return not self.InParentheses()

  def GetBlockType(self, token):
    """Determine the block type given a START_BLOCK token.

    Code blocks come after parameters, keywords  like else, and closing parens.

    Args:
      token: The current token. Can be assumed to be type START_BLOCK
    Returns:
      Code block type for current token.
    """
    last_code = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES, None,
                                       True)
    if last_code.type in (Type.END_PARAMETERS, Type.END_PAREN,
                          Type.KEYWORD) and not last_code.IsKeyword('return'):
      return self.CODE
    else:
      return self.OBJECT_LITERAL

  def HandleToken(self, token, last_non_space_token):
    """Handles the given token and updates state.

    Args:
      token: The token to handle.
      last_non_space_token:
    """
    super(JavaScriptStateTracker, self).HandleToken(token,
                                                    last_non_space_token)
