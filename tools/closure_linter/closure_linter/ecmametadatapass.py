#!/usr/bin/env python
#
# Copyright 2010 The Closure Linter Authors. All Rights Reserved.
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

"""Metadata pass for annotating tokens in EcmaScript files."""

__author__ = ('robbyw@google.com (Robert Walker)')

from closure_linter import javascripttokens
from closure_linter import tokenutil


TokenType = javascripttokens.JavaScriptTokenType


class ParseError(Exception):
  """Exception indicating a parse error at the given token.

  Attributes:
    token: The token where the parse error occurred.
  """

  def __init__(self, token, message=None):
    """Initialize a parse error at the given token with an optional message.

    Args:
      token: The token where the parse error occurred.
      message: A message describing the parse error.
    """
    Exception.__init__(self, message)
    self.token = token


class EcmaContext(object):
  """Context object for EcmaScript languages.

  Attributes:
    type: The context type.
    start_token: The token where this context starts.
    end_token: The token where this context ends.
    parent: The parent context.
  """

  # The root context.
  ROOT = 'root'

  # A block of code.
  BLOCK = 'block'

  # A pseudo-block of code for a given case or default section.
  CASE_BLOCK = 'case_block'

  # Block of statements in a for loop's parentheses.
  FOR_GROUP_BLOCK = 'for_block'

  # An implied block of code for 1 line if, while, and for statements
  IMPLIED_BLOCK = 'implied_block'

  # An index in to an array or object.
  INDEX = 'index'

  # An array literal in [].
  ARRAY_LITERAL = 'array_literal'

  # An object literal in {}.
  OBJECT_LITERAL = 'object_literal'

  # An individual element in an array or object literal.
  LITERAL_ELEMENT = 'literal_element'

  # The portion of a ternary statement between ? and :
  TERNARY_TRUE = 'ternary_true'

  # The portion of a ternary statment after :
  TERNARY_FALSE = 'ternary_false'

  # The entire switch statment.  This will contain a GROUP with the variable
  # and a BLOCK with the code.

  # Since that BLOCK is not a normal block, it can not contain statements except
  # for case and default.
  SWITCH = 'switch'

  # A normal comment.
  COMMENT = 'comment'

  # A JsDoc comment.
  DOC = 'doc'

  # An individual statement.
  STATEMENT = 'statement'

  # Code within parentheses.
  GROUP = 'group'

  # Parameter names in a function declaration.
  PARAMETERS = 'parameters'

  # A set of variable declarations appearing after the 'var' keyword.
  VAR = 'var'

  # Context types that are blocks.
  BLOCK_TYPES = frozenset([
      ROOT, BLOCK, CASE_BLOCK, FOR_GROUP_BLOCK, IMPLIED_BLOCK])

  def __init__(self, type, start_token, parent):
    """Initializes the context object.

    Args:
      type: The context type.
      start_token: The token where this context starts.
      parent: The parent context.
    """
    self.type = type
    self.start_token = start_token
    self.end_token = None
    self.parent = parent

  def __repr__(self):
    """Returns a string representation of the context object."""
    stack = []
    context = self
    while context:
      stack.append(context.type)
      context = context.parent
    return 'Context(%s)' % ' > '.join(stack)


class EcmaMetaData(object):
  """Token metadata for EcmaScript languages.

  Attributes:
    last_code: The last code token to appear before this one.
    context: The context this token appears in.
    operator_type: The operator type, will be one of the *_OPERATOR constants
        defined below.
  """

  UNARY_OPERATOR = 'unary'

  UNARY_POST_OPERATOR = 'unary_post'

  BINARY_OPERATOR = 'binary'

  TERNARY_OPERATOR = 'ternary'

  def __init__(self):
    """Initializes a token metadata object."""
    self.last_code = None
    self.context = None
    self.operator_type = None
    self.is_implied_semicolon = False
    self.is_implied_block = False
    self.is_implied_block_close = False

  def __repr__(self):
    """Returns a string representation of the context object."""
    parts = ['%r' % self.context]
    if self.operator_type:
      parts.append('optype: %r' % self.operator_type)
    if self.is_implied_semicolon:
      parts.append('implied;')
    return 'MetaData(%s)' % ', '.join(parts)

  def IsUnaryOperator(self):
    return self.operator_type in (EcmaMetaData.UNARY_OPERATOR,
                                  EcmaMetaData.UNARY_POST_OPERATOR)

  def IsUnaryPostOperator(self):
    return self.operator_type == EcmaMetaData.UNARY_POST_OPERATOR


class EcmaMetaDataPass(object):
  """A pass that iterates over all tokens and builds metadata about them."""

  def __init__(self):
    """Initialize the meta data pass object."""
    self.Reset()

  def Reset(self):
    """Resets the metadata pass to prepare for the next file."""
    self._token = None
    self._context = None
    self._AddContext(EcmaContext.ROOT)
    self._last_code = None

  def _CreateContext(self, type):
    """Overridable by subclasses to create the appropriate context type."""
    return EcmaContext(type, self._token, self._context)

  def _CreateMetaData(self):
    """Overridable by subclasses to create the appropriate metadata type."""
    return EcmaMetaData()

  def _AddContext(self, type):
    """Adds a context of the given type to the context stack.

    Args:
      type: The type of context to create
    """
    self._context  = self._CreateContext(type)

  def _PopContext(self):
    """Moves up one level in the context stack.

    Returns:
      The former context.

    Raises:
      ParseError: If the root context is popped.
    """
    top_context = self._context
    top_context.end_token = self._token
    self._context = top_context.parent
    if self._context:
      return top_context
    else:
      raise ParseError(self._token)

  def _PopContextType(self, *stop_types):
    """Pops the context stack until a context of the given type is popped.

    Args:
      stop_types: The types of context to pop to - stops at the first match.

    Returns:
      The context object of the given type that was popped.
    """
    last = None
    while not last or last.type not in stop_types:
      last = self._PopContext()
    return last

  def _EndStatement(self):
    """Process the end of a statement."""
    self._PopContextType(EcmaContext.STATEMENT)
    if self._context.type == EcmaContext.IMPLIED_BLOCK:
      self._token.metadata.is_implied_block_close = True
      self._PopContext()

  def _ProcessContext(self):
    """Process the context at the current token.

    Returns:
      The context that should be assigned to the current token, or None if
      the current context after this method should be used.

    Raises:
      ParseError: When the token appears in an invalid context.
    """
    token = self._token
    token_type = token.type

    if self._context.type in EcmaContext.BLOCK_TYPES:
      # Whenever we're in a block, we add a statement context.  We make an
      # exception for switch statements since they can only contain case: and
      # default: and therefore don't directly contain statements.
      # The block we add here may be immediately removed in some cases, but
      # that causes no harm.
      parent = self._context.parent
      if not parent or parent.type != EcmaContext.SWITCH:
        self._AddContext(EcmaContext.STATEMENT)

    elif self._context.type == EcmaContext.ARRAY_LITERAL:
      self._AddContext(EcmaContext.LITERAL_ELEMENT)

    if token_type == TokenType.START_PAREN:
      if self._last_code and self._last_code.IsKeyword('for'):
        # for loops contain multiple statements in the group unlike while,
        # switch, if, etc.
        self._AddContext(EcmaContext.FOR_GROUP_BLOCK)
      else:
        self._AddContext(EcmaContext.GROUP)

    elif token_type == TokenType.END_PAREN:
      result = self._PopContextType(EcmaContext.GROUP,
                                    EcmaContext.FOR_GROUP_BLOCK)
      keyword_token = result.start_token.metadata.last_code
      # keyword_token will not exist if the open paren is the first line of the
      # file, for example if all code is wrapped in an immediately executed
      # annonymous function.
      if keyword_token and keyword_token.string in ('if', 'for', 'while'):
        next_code = tokenutil.SearchExcept(token, TokenType.NON_CODE_TYPES)
        if next_code.type != TokenType.START_BLOCK:
          # Check for do-while.
          is_do_while = False
          pre_keyword_token = keyword_token.metadata.last_code
          if (pre_keyword_token and
              pre_keyword_token.type == TokenType.END_BLOCK):
            start_block_token = pre_keyword_token.metadata.context.start_token
            is_do_while = start_block_token.metadata.last_code.string == 'do'

          # If it's not do-while, it's an implied block.
          if not is_do_while:
            self._AddContext(EcmaContext.IMPLIED_BLOCK)
            token.metadata.is_implied_block = True

      return result

    # else (not else if) with no open brace after it should be considered the
    # start of an implied block, similar to the case with if, for, and while
    # above.
    elif (token_type == TokenType.KEYWORD and
          token.string == 'else'):
      next_code = tokenutil.SearchExcept(token, TokenType.NON_CODE_TYPES)
      if (next_code.type != TokenType.START_BLOCK and
          (next_code.type != TokenType.KEYWORD or next_code.string != 'if')):
        self._AddContext(EcmaContext.IMPLIED_BLOCK)
        token.metadata.is_implied_block = True

    elif token_type == TokenType.START_PARAMETERS:
      self._AddContext(EcmaContext.PARAMETERS)

    elif token_type == TokenType.END_PARAMETERS:
      return self._PopContextType(EcmaContext.PARAMETERS)

    elif token_type == TokenType.START_BRACKET:
      if (self._last_code and
          self._last_code.type in TokenType.EXPRESSION_ENDER_TYPES):
        self._AddContext(EcmaContext.INDEX)
      else:
        self._AddContext(EcmaContext.ARRAY_LITERAL)

    elif token_type == TokenType.END_BRACKET:
      return self._PopContextType(EcmaContext.INDEX, EcmaContext.ARRAY_LITERAL)

    elif token_type == TokenType.START_BLOCK:
      if (self._last_code.type in (TokenType.END_PAREN,
                                   TokenType.END_PARAMETERS) or
          self._last_code.IsKeyword('else') or
          self._last_code.IsKeyword('do') or
          self._last_code.IsKeyword('try') or
          self._last_code.IsKeyword('finally') or
          (self._last_code.IsOperator(':') and
           self._last_code.metadata.context.type == EcmaContext.CASE_BLOCK)):
        # else, do, try, and finally all might have no () before {.
        # Also, handle the bizzare syntax case 10: {...}.
        self._AddContext(EcmaContext.BLOCK)
      else:
        self._AddContext(EcmaContext.OBJECT_LITERAL)

    elif token_type == TokenType.END_BLOCK:
      context = self._PopContextType(EcmaContext.BLOCK,
                                     EcmaContext.OBJECT_LITERAL)
      if self._context.type == EcmaContext.SWITCH:
        # The end of the block also means the end of the switch statement it
        # applies to.
        return self._PopContext()
      return context

    elif token.IsKeyword('switch'):
      self._AddContext(EcmaContext.SWITCH)

    elif (token_type == TokenType.KEYWORD and
          token.string in ('case', 'default')):
      # Pop up to but not including the switch block.
      while self._context.parent.type != EcmaContext.SWITCH:
        self._PopContext()

    elif token.IsOperator('?'):
      self._AddContext(EcmaContext.TERNARY_TRUE)

    elif token.IsOperator(':'):
      if self._context.type == EcmaContext.OBJECT_LITERAL:
        self._AddContext(EcmaContext.LITERAL_ELEMENT)

      elif self._context.type == EcmaContext.TERNARY_TRUE:
        self._PopContext()
        self._AddContext(EcmaContext.TERNARY_FALSE)

      # Handle nested ternary statements like:
      # foo = bar ? baz ? 1 : 2 : 3
      # When we encounter the second ":" the context is
      # ternary_false > ternary_true > statement > root
      elif (self._context.type == EcmaContext.TERNARY_FALSE and
            self._context.parent.type == EcmaContext.TERNARY_TRUE):
           self._PopContext() # Leave current ternary false context.
           self._PopContext() # Leave current parent ternary true
           self._AddContext(EcmaContext.TERNARY_FALSE)

      elif self._context.parent.type == EcmaContext.SWITCH:
        self._AddContext(EcmaContext.CASE_BLOCK)

    elif token.IsKeyword('var'):
      self._AddContext(EcmaContext.VAR)

    elif token.IsOperator(','):
      while self._context.type not in (EcmaContext.VAR,
                                       EcmaContext.ARRAY_LITERAL,
                                       EcmaContext.OBJECT_LITERAL,
                                       EcmaContext.STATEMENT,
                                       EcmaContext.PARAMETERS,
                                       EcmaContext.GROUP):
        self._PopContext()

    elif token_type == TokenType.SEMICOLON:
      self._EndStatement()

  def Process(self, first_token):
    """Processes the token stream starting with the given token."""
    self._token = first_token
    while self._token:
      self._ProcessToken()

      if self._token.IsCode():
        self._last_code = self._token

      self._token = self._token.next

    try:
      self._PopContextType(self, EcmaContext.ROOT)
    except ParseError:
      # Ignore the "popped to root" error.
      pass

  def _ProcessToken(self):
    """Process the given token."""
    token = self._token
    token.metadata = self._CreateMetaData()
    context = (self._ProcessContext() or self._context)
    token.metadata.context = context
    token.metadata.last_code = self._last_code

    # Determine the operator type of the token, if applicable.
    if token.type == TokenType.OPERATOR:
      token.metadata.operator_type = self._GetOperatorType(token)

    # Determine if there is an implied semicolon after the token.
    if token.type != TokenType.SEMICOLON:
      next_code = tokenutil.SearchExcept(token, TokenType.NON_CODE_TYPES)
      # A statement like if (x) does not need a semicolon after it
      is_implied_block = self._context == EcmaContext.IMPLIED_BLOCK
      is_last_code_in_line = token.IsCode() and (
          not next_code or next_code.line_number != token.line_number)
      is_continued_identifier = (token.type == TokenType.IDENTIFIER and
                                 token.string.endswith('.'))
      is_continued_operator = (token.type == TokenType.OPERATOR and
                               not token.metadata.IsUnaryPostOperator())
      is_continued_dot = token.string == '.'
      next_code_is_operator = next_code and next_code.type == TokenType.OPERATOR
      next_code_is_dot = next_code and next_code.string == '.'
      is_end_of_block = (token.type == TokenType.END_BLOCK and
          token.metadata.context.type != EcmaContext.OBJECT_LITERAL)
      is_multiline_string = token.type == TokenType.STRING_TEXT
      next_code_is_block = next_code and next_code.type == TokenType.START_BLOCK
      if (is_last_code_in_line and
          self._StatementCouldEndInContext() and
          not is_multiline_string and
          not is_end_of_block and
          not is_continued_identifier and
          not is_continued_operator and
          not is_continued_dot and
          not next_code_is_dot and
          not next_code_is_operator and
          not is_implied_block and
          not next_code_is_block):
        token.metadata.is_implied_semicolon = True
        self._EndStatement()

  def _StatementCouldEndInContext(self):
    """Returns whether the current statement (if any) may end in this context."""
    # In the basic statement or variable declaration context, statement can
    # always end in this context.
    if self._context.type in (EcmaContext.STATEMENT, EcmaContext.VAR):
      return True

    # End of a ternary false branch inside a statement can also be the
    # end of the statement, for example:
    # var x = foo ? foo.bar() : null
    # In this case the statement ends after the null, when the context stack
    # looks like ternary_false > var > statement > root.
    if (self._context.type == EcmaContext.TERNARY_FALSE and
        self._context.parent.type in (EcmaContext.STATEMENT, EcmaContext.VAR)):
      return True

    # In all other contexts like object and array literals, ternary true, etc.
    # the statement can't yet end.
    return False

  def _GetOperatorType(self, token):
    """Returns the operator type of the given operator token.

    Args:
      token: The token to get arity for.

    Returns:
      The type of the operator.  One of the *_OPERATOR constants defined in
      EcmaMetaData.
    """
    if token.string == '?':
      return EcmaMetaData.TERNARY_OPERATOR

    if token.string in TokenType.UNARY_OPERATORS:
      return EcmaMetaData.UNARY_OPERATOR

    last_code = token.metadata.last_code
    if not last_code or last_code.type == TokenType.END_BLOCK:
      return EcmaMetaData.UNARY_OPERATOR

    if (token.string in TokenType.UNARY_POST_OPERATORS and
        last_code.type in TokenType.EXPRESSION_ENDER_TYPES):
      return EcmaMetaData.UNARY_POST_OPERATOR

    if (token.string in TokenType.UNARY_OK_OPERATORS and
        last_code.type not in TokenType.EXPRESSION_ENDER_TYPES and
        last_code.string not in TokenType.UNARY_POST_OPERATORS):
      return EcmaMetaData.UNARY_OPERATOR

    return EcmaMetaData.BINARY_OPERATOR
