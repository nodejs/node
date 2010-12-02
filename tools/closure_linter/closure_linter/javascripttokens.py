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

"""Classes to represent JavaScript tokens."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

from closure_linter.common import tokens

class JavaScriptTokenType(tokens.TokenType):
  """Enumeration of JavaScript token types, and useful sets of token types."""
  NUMBER = 'number'
  START_SINGLE_LINE_COMMENT = '//'
  START_BLOCK_COMMENT = '/*'
  START_DOC_COMMENT = '/**'
  END_BLOCK_COMMENT = '*/'
  END_DOC_COMMENT = 'doc */'
  COMMENT = 'comment'
  SINGLE_QUOTE_STRING_START = "'string"
  SINGLE_QUOTE_STRING_END = "string'"
  DOUBLE_QUOTE_STRING_START = '"string'
  DOUBLE_QUOTE_STRING_END = 'string"'
  STRING_TEXT = 'string'
  START_BLOCK = '{'
  END_BLOCK = '}'
  START_PAREN = '('
  END_PAREN = ')'
  START_BRACKET = '['
  END_BRACKET = ']'
  REGEX = '/regex/'
  FUNCTION_DECLARATION = 'function(...)'
  FUNCTION_NAME = 'function functionName(...)'
  START_PARAMETERS = 'startparams('
  PARAMETERS = 'pa,ra,ms'
  END_PARAMETERS = ')endparams'
  SEMICOLON = ';'
  DOC_FLAG = '@flag'
  DOC_INLINE_FLAG = '{@flag ...}'
  DOC_START_BRACE = 'doc {'
  DOC_END_BRACE = 'doc }'
  DOC_PREFIX = 'comment prefix: * '
  SIMPLE_LVALUE = 'lvalue='
  KEYWORD = 'keyword'
  OPERATOR = 'operator'
  IDENTIFIER = 'identifier'

  STRING_TYPES = frozenset([
      SINGLE_QUOTE_STRING_START, SINGLE_QUOTE_STRING_END,
      DOUBLE_QUOTE_STRING_START, DOUBLE_QUOTE_STRING_END, STRING_TEXT])

  COMMENT_TYPES = frozenset([START_SINGLE_LINE_COMMENT, COMMENT,
      START_BLOCK_COMMENT, START_DOC_COMMENT,
      END_BLOCK_COMMENT, END_DOC_COMMENT,
      DOC_START_BRACE, DOC_END_BRACE,
      DOC_FLAG, DOC_INLINE_FLAG, DOC_PREFIX])

  FLAG_DESCRIPTION_TYPES = frozenset([
      DOC_INLINE_FLAG, COMMENT, DOC_START_BRACE, DOC_END_BRACE])

  FLAG_ENDING_TYPES = frozenset([DOC_FLAG, END_DOC_COMMENT])

  NON_CODE_TYPES = COMMENT_TYPES | frozenset([
      tokens.TokenType.WHITESPACE, tokens.TokenType.BLANK_LINE])

  UNARY_OPERATORS = ['!', 'new', 'delete', 'typeof', 'void']

  UNARY_OK_OPERATORS = ['--', '++', '-', '+'] + UNARY_OPERATORS

  UNARY_POST_OPERATORS = ['--', '++']

  # An expression ender is any token that can end an object - i.e. we could have
  # x.y or [1, 2], or (10 + 9) or {a: 10}.
  EXPRESSION_ENDER_TYPES = [tokens.TokenType.NORMAL, IDENTIFIER, NUMBER,
                            SIMPLE_LVALUE, END_BRACKET, END_PAREN, END_BLOCK,
                            SINGLE_QUOTE_STRING_END, DOUBLE_QUOTE_STRING_END]


class JavaScriptToken(tokens.Token):
  """JavaScript token subclass of Token, provides extra instance checks.

  The following token types have data in attached_object:
    - All JsDoc flags: a parser.JsDocFlag object.
  """

  def IsKeyword(self, keyword):
    """Tests if this token is the given keyword.

    Args:
      keyword: The keyword to compare to.

    Returns:
      True if this token is a keyword token with the given name.
    """
    return self.type == JavaScriptTokenType.KEYWORD and self.string == keyword

  def IsOperator(self, operator):
    """Tests if this token is the given operator.

    Args:
      operator: The operator to compare to.

    Returns:
      True if this token is a operator token with the given name.
    """
    return self.type == JavaScriptTokenType.OPERATOR and self.string == operator

  def IsAssignment(self):
    """Tests if this token is an assignment operator.

    Returns:
      True if this token is an assignment operator.
    """
    return (self.type == JavaScriptTokenType.OPERATOR and
            self.string.endswith('=') and
            self.string not in ('==', '!=', '>=', '<=', '===', '!=='))

  def IsComment(self):
    """Tests if this token is any part of a comment.

    Returns:
      True if this token is any part of a comment.
    """
    return self.type in JavaScriptTokenType.COMMENT_TYPES

  def IsCode(self):
    """Tests if this token is code, as opposed to a comment or whitespace."""
    return self.type not in JavaScriptTokenType.NON_CODE_TYPES

  def __repr__(self):
    return '<JavaScriptToken: %d, %s, "%s", %r, %r>' % (self.line_number,
                                                        self.type, self.string,
                                                        self.values,
                                                        self.metadata)
