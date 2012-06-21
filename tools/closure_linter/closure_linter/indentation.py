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

"""Methods for checking EcmaScript files for indentation issues."""

__author__ = ('robbyw@google.com (Robert Walker)')

from closure_linter import ecmametadatapass
from closure_linter import errors
from closure_linter import javascripttokens
from closure_linter import tokenutil
from closure_linter.common import error
from closure_linter.common import position

import gflags as flags

flags.DEFINE_boolean('debug_indentation', False,
                     'Whether to print debugging information for indentation.')


# Shorthand
Context = ecmametadatapass.EcmaContext
Error = error.Error
Position = position.Position
Type = javascripttokens.JavaScriptTokenType


# The general approach:
#
# 1. Build a stack of tokens that can affect indentation.
#    For each token, we determine if it is a block or continuation token.
#    Some tokens need to be temporarily overwritten in case they are removed
#    before the end of the line.
#    Much of the work here is determining which tokens to keep on the stack
#    at each point.  Operators, for example, should be removed once their
#    expression or line is gone, while parentheses must stay until the matching
#    end parentheses is found.
#
# 2. Given that stack, determine the allowable indentations.
#    Due to flexible indentation rules in JavaScript, there may be many
#    allowable indentations for each stack.  We follows the general
#    "no false positives" approach of GJsLint and build the most permissive
#    set possible.


class TokenInfo(object):
  """Stores information about a token.

  Attributes:
    token: The token
    is_block: Whether the token represents a block indentation.
    is_transient: Whether the token should be automatically removed without
      finding a matching end token.
    overridden_by: TokenInfo for a token that overrides the indentation that
      this token would require.
    is_permanent_override: Whether the override on this token should persist
      even after the overriding token is removed from the stack.  For example:
      x([
        1],
      2);
      needs this to be set so the last line is not required to be a continuation
      indent.
    line_number: The effective line number of this token.  Will either be the
      actual line number or the one before it in the case of a mis-wrapped
      operator.
  """

  def __init__(self, token, is_block=False):
    """Initializes a TokenInfo object.

    Args:
      token: The token
      is_block: Whether the token represents a block indentation.
    """
    self.token = token
    self.overridden_by = None
    self.is_permanent_override = False
    self.is_block = is_block
    self.is_transient = not is_block and not token.type in (
        Type.START_PAREN, Type.START_PARAMETERS)
    self.line_number = token.line_number

  def __repr__(self):
    result = '\n  %s' % self.token
    if self.overridden_by:
      result = '%s OVERRIDDEN [by "%s"]' % (
          result, self.overridden_by.token.string)
    result += ' {is_block: %s, is_transient: %s}' % (
        self.is_block, self.is_transient)
    return result


class IndentationRules(object):
  """EmcaScript indentation rules.

  Can be used to find common indentation errors in JavaScript, ActionScript and
  other Ecma like scripting languages.
  """

  def __init__(self):
    """Initializes the IndentationRules checker."""
    self._stack = []

    # Map from line number to number of characters it is off in indentation.
    self._start_index_offset = {}

  def Finalize(self):
    if self._stack:
      old_stack = self._stack
      self._stack = []
      raise Exception("INTERNAL ERROR: indentation stack is not empty: %r" %
                      old_stack)

  def CheckToken(self, token, state):
    """Checks a token for indentation errors.

    Args:
      token: The current token under consideration
      state: Additional information about the current tree state

    Returns:
      An error array [error code, error string, error token] if the token is
      improperly indented, or None if indentation is correct.
    """

    token_type = token.type
    indentation_errors = []
    stack = self._stack
    is_first = self._IsFirstNonWhitespaceTokenInLine(token)

    # Add tokens that could decrease indentation before checking.
    if token_type == Type.END_PAREN:
      self._PopTo(Type.START_PAREN)

    elif token_type == Type.END_PARAMETERS:
      self._PopTo(Type.START_PARAMETERS)

    elif token_type == Type.END_BRACKET:
      self._PopTo(Type.START_BRACKET)

    elif token_type == Type.END_BLOCK:
      start_token = self._PopTo(Type.START_BLOCK)
      # Check for required goog.scope comment.
      if start_token:
        goog_scope = self._GoogScopeOrNone(start_token.token)
        if goog_scope is not None:
          if not token.line.endswith(';  // goog.scope\n'):
            if (token.line.find('//') > -1 and
                token.line.find('goog.scope') >
                token.line.find('//')):
              indentation_errors.append([
                  errors.MALFORMED_END_OF_SCOPE_COMMENT,
                  ('Malformed end of goog.scope comment. Please use the '
                   'exact following syntax to close the scope:\n'
                   '});  // goog.scope'),
                  token,
                  Position(token.start_index, token.length)])
            else:
              indentation_errors.append([
                  errors.MISSING_END_OF_SCOPE_COMMENT,
                  ('Missing comment for end of goog.scope which opened at line '
                   '%d. End the scope with:\n'
                   '});  // goog.scope' %
                   (start_token.line_number)),
                  token,
                  Position(token.start_index, token.length)])

    elif token_type == Type.KEYWORD and token.string in ('case', 'default'):
      self._Add(self._PopTo(Type.START_BLOCK))

    elif is_first and token.string == '.':
      # This token should have been on the previous line, so treat it as if it
      # was there.
      info = TokenInfo(token)
      info.line_number = token.line_number - 1
      self._Add(info)

    elif token_type == Type.SEMICOLON:
      self._PopTransient()

    not_binary_operator = (token_type != Type.OPERATOR or
                           token.metadata.IsUnaryOperator())
    not_dot = token.string != '.'
    if is_first and not_binary_operator and not_dot and token.type not in (
        Type.COMMENT, Type.DOC_PREFIX, Type.STRING_TEXT):
      if flags.FLAGS.debug_indentation:
        print 'Line #%d: stack %r' % (token.line_number, stack)

      # Ignore lines that start in JsDoc since we don't check them properly yet.
      # TODO(robbyw): Support checking JsDoc indentation.
      # Ignore lines that start as multi-line strings since indentation is N/A.
      # Ignore lines that start with operators since we report that already.
      # Ignore lines with tabs since we report that already.
      expected = self._GetAllowableIndentations()
      actual = self._GetActualIndentation(token)

      # Special case comments describing else, case, and default.  Allow them
      # to outdent to the parent block.
      if token_type in Type.COMMENT_TYPES:
        next_code = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES)
        if next_code and next_code.type == Type.END_BLOCK:
          next_code = tokenutil.SearchExcept(next_code, Type.NON_CODE_TYPES)
        if next_code and next_code.string in ('else', 'case', 'default'):
          # TODO(robbyw): This almost certainly introduces false negatives.
          expected |= self._AddToEach(expected, -2)

      if actual >= 0 and actual not in expected:
        expected = sorted(expected)
        indentation_errors.append([
            errors.WRONG_INDENTATION,
            'Wrong indentation: expected any of {%s} but got %d' % (
                ', '.join(
                    ['%d' % x for x in expected]), actual),
            token,
            Position(actual, expected[0])])
        self._start_index_offset[token.line_number] = expected[0] - actual

    # Add tokens that could increase indentation.
    if token_type == Type.START_BRACKET:
      self._Add(TokenInfo(token=token,
          is_block=token.metadata.context.type == Context.ARRAY_LITERAL))

    elif token_type == Type.START_BLOCK or token.metadata.is_implied_block:
      self._Add(TokenInfo(token=token, is_block=True))

    elif token_type in (Type.START_PAREN, Type.START_PARAMETERS):
      self._Add(TokenInfo(token=token, is_block=False))

    elif token_type == Type.KEYWORD and token.string == 'return':
      self._Add(TokenInfo(token))

    elif not token.IsLastInLine() and (
        token.IsAssignment() or token.IsOperator('?')):
      self._Add(TokenInfo(token=token))

    # Handle implied block closes.
    if token.metadata.is_implied_block_close:
      self._PopToImpliedBlock()

    # Add some tokens only if they appear at the end of the line.
    is_last = self._IsLastCodeInLine(token)
    if is_last:
      if token_type == Type.OPERATOR:
        if token.string == ':':
          if (stack and stack[-1].token.string == '?'):
            # When a ternary : is on a different line than its '?', it doesn't
            # add indentation.
            if (token.line_number == stack[-1].token.line_number):
              self._Add(TokenInfo(token))
          elif token.metadata.context.type == Context.CASE_BLOCK:
            # Pop transient tokens from say, line continuations, e.g.,
            # case x.
            #     y:
            # Want to pop the transient 4 space continuation indent.
            self._PopTransient()
            # Starting the body of the case statement, which is a type of
            # block.
            self._Add(TokenInfo(token=token, is_block=True))
          elif token.metadata.context.type == Context.LITERAL_ELEMENT:
            # When in an object literal, acts as operator indicating line
            # continuations.
            self._Add(TokenInfo(token))
            pass
          else:
            # ':' might also be a statement label, no effect on indentation in
            # this case.
            pass

        elif token.string != ',':
          self._Add(TokenInfo(token))
        else:
          # The token is a comma.
          if token.metadata.context.type == Context.VAR:
            self._Add(TokenInfo(token))
          elif token.metadata.context.type != Context.PARAMETERS:
            self._PopTransient()

      elif (token.string.endswith('.')
            and token_type in (Type.IDENTIFIER, Type.NORMAL)):
        self._Add(TokenInfo(token))
      elif token_type == Type.PARAMETERS and token.string.endswith(','):
        # Parameter lists.
        self._Add(TokenInfo(token))
      elif token.metadata.is_implied_semicolon:
        self._PopTransient()
    elif token.IsAssignment():
      self._Add(TokenInfo(token))

    return indentation_errors

  def _AddToEach(self, original, amount):
    """Returns a new set with the given amount added to each element.

    Args:
      original: The original set of numbers
      amount: The amount to add to each element

    Returns:
      A new set containing each element of the original set added to the amount.
    """
    return set([x + amount for x in original])

  _HARD_STOP_TYPES = (Type.START_PAREN, Type.START_PARAMETERS,
                      Type.START_BRACKET)

  _HARD_STOP_STRINGS = ('return', '?')

  def _IsHardStop(self, token):
    """Determines if the given token can have a hard stop after it.

    Hard stops are indentations defined by the position of another token as in
    indentation lined up with return, (, [, and ?.
    """
    return (token.type in self._HARD_STOP_TYPES or
            token.string in self._HARD_STOP_STRINGS or
            token.IsAssignment())

  def _GetAllowableIndentations(self):
    """Computes the set of allowable indentations.

    Returns:
      The set of allowable indentations, given the current stack.
    """
    expected = set([0])
    hard_stops = set([])

    # Whether the tokens are still in the same continuation, meaning additional
    # indentation is optional.  As an example:
    # x = 5 +
    #     6 +
    #     7;
    # The second '+' does not add any required indentation.
    in_same_continuation = False

    for token_info in self._stack:
      token = token_info.token

      # Handle normal additive indentation tokens.
      if not token_info.overridden_by and token.string != 'return':
        if token_info.is_block:
          expected = self._AddToEach(expected, 2)
          hard_stops = self._AddToEach(hard_stops, 2)
          in_same_continuation = False
        elif in_same_continuation:
          expected |= self._AddToEach(expected, 4)
          hard_stops |= self._AddToEach(hard_stops, 4)
        else:
          expected = self._AddToEach(expected, 4)
          hard_stops |= self._AddToEach(hard_stops, 4)
          in_same_continuation = True

      # Handle hard stops after (, [, return, =, and ?
      if self._IsHardStop(token):
        override_is_hard_stop = (token_info.overridden_by and
            self._IsHardStop(token_info.overridden_by.token))
        if not override_is_hard_stop:
          start_index = token.start_index
          if token.line_number in self._start_index_offset:
            start_index += self._start_index_offset[token.line_number]
          if (token.type in (Type.START_PAREN, Type.START_PARAMETERS) and
              not token_info.overridden_by):
            hard_stops.add(start_index + 1)

          elif token.string == 'return' and not token_info.overridden_by:
            hard_stops.add(start_index + 7)

          elif (token.type == Type.START_BRACKET):
            hard_stops.add(start_index + 1)

          elif token.IsAssignment():
            hard_stops.add(start_index + len(token.string) + 1)

          elif token.IsOperator('?') and not token_info.overridden_by:
            hard_stops.add(start_index + 2)

    return (expected | hard_stops) or set([0])

  def _GetActualIndentation(self, token):
    """Gets the actual indentation of the line containing the given token.

    Args:
      token: Any token on the line.

    Returns:
      The actual indentation of the line containing the given token.  Returns
      -1 if this line should be ignored due to the presence of tabs.
    """
    # Move to the first token in the line
    token = tokenutil.GetFirstTokenInSameLine(token)

    # If it is whitespace, it is the indentation.
    if token.type == Type.WHITESPACE:
      if token.string.find('\t') >= 0:
        return -1
      else:
        return len(token.string)
    elif token.type == Type.PARAMETERS:
      return len(token.string) - len(token.string.lstrip())
    else:
      return 0

  def _IsFirstNonWhitespaceTokenInLine(self, token):
    """Determines if the given token is the first non-space token on its line.

    Args:
      token: The token.

    Returns:
      True if the token is the first non-whitespace token on its line.
    """
    if token.type in (Type.WHITESPACE, Type.BLANK_LINE):
      return False
    if token.IsFirstInLine():
      return True
    return (token.previous and token.previous.IsFirstInLine() and
            token.previous.type == Type.WHITESPACE)

  def _IsLastCodeInLine(self, token):
    """Determines if the given token is the last code token on its line.

    Args:
      token: The token.

    Returns:
      True if the token is the last code token on its line.
    """
    if token.type in Type.NON_CODE_TYPES:
      return False
    start_token = token
    while True:
      token = token.next
      if not token or token.line_number != start_token.line_number:
        return True
      if token.type not in Type.NON_CODE_TYPES:
        return False

  def _GoogScopeOrNone(self, token):
    """Determines if the given START_BLOCK is part of a goog.scope statement.

    Args:
      token: A token of type START_BLOCK.

    Returns:
      The goog.scope function call token, or None if such call doesn't exist.
    """
    # Search for a goog.scope statement, which will be 5 tokens before the
    # block. Illustration of the tokens found prior to the start block:
    # goog.scope(function() {
    #      5    4    3   21 ^

    maybe_goog_scope = token
    for unused_i in xrange(5):
      maybe_goog_scope = (maybe_goog_scope.previous if maybe_goog_scope and
                          maybe_goog_scope.previous else None)
    if maybe_goog_scope and maybe_goog_scope.string == 'goog.scope':
      return maybe_goog_scope

  def _Add(self, token_info):
    """Adds the given token info to the stack.

    Args:
      token_info: The token information to add.
    """
    if self._stack and self._stack[-1].token == token_info.token:
      # Don't add the same token twice.
      return

    if token_info.is_block or token_info.token.type == Type.START_PAREN:
      token_info.overridden_by = self._GoogScopeOrNone(token_info.token)
      index = 1
      while index <= len(self._stack):
        stack_info = self._stack[-index]
        stack_token = stack_info.token

        if stack_info.line_number == token_info.line_number:
          # In general, tokens only override each other when they are on
          # the same line.
          stack_info.overridden_by = token_info
          if (token_info.token.type == Type.START_BLOCK and
              (stack_token.IsAssignment() or
               stack_token.type in (Type.IDENTIFIER, Type.START_PAREN))):
            # Multi-line blocks have lasting overrides, as in:
            # callFn({
            #   a: 10
            # },
            # 30);
            close_block = token_info.token.metadata.context.end_token
            stack_info.is_permanent_override = \
                close_block.line_number != token_info.token.line_number
        elif (token_info.token.type == Type.START_BLOCK and
              token_info.token.metadata.context.type == Context.BLOCK and
              (stack_token.IsAssignment() or
               stack_token.type == Type.IDENTIFIER)):
          # When starting a function block, the override can transcend lines.
          # For example
          # long.long.name = function(
          #     a) {
          # In this case the { and the = are on different lines.  But the
          # override should still apply.
          stack_info.overridden_by = token_info
          stack_info.is_permanent_override = True
        else:
          break
        index += 1

    self._stack.append(token_info)

  def _Pop(self):
    """Pops the top token from the stack.

    Returns:
      The popped token info.
    """
    token_info = self._stack.pop()
    if token_info.token.type not in (Type.START_BLOCK, Type.START_BRACKET):
      # Remove any temporary overrides.
      self._RemoveOverrides(token_info)
    else:
      # For braces and brackets, which can be object and array literals, remove
      # overrides when the literal is closed on the same line.
      token_check = token_info.token
      same_type = token_check.type
      goal_type = None
      if token_info.token.type == Type.START_BRACKET:
        goal_type = Type.END_BRACKET
      else:
        goal_type = Type.END_BLOCK
      line_number = token_info.token.line_number
      count = 0
      while token_check and token_check.line_number == line_number:
        if token_check.type == goal_type:
          count -= 1
          if not count:
            self._RemoveOverrides(token_info)
            break
        if token_check.type == same_type:
          count += 1
        token_check = token_check.next
    return token_info

  def _PopToImpliedBlock(self):
    """Pops the stack until an implied block token is found."""
    while not self._Pop().token.metadata.is_implied_block:
      pass

  def _PopTo(self, stop_type):
    """Pops the stack until a token of the given type is popped.

    Args:
      stop_type: The type of token to pop to.

    Returns:
      The token info of the given type that was popped.
    """
    last = None
    while True:
      last = self._Pop()
      if last.token.type == stop_type:
        break
    return last

  def _RemoveOverrides(self, token_info):
    """Marks any token that was overridden by this token as active again.

    Args:
      token_info: The token that is being removed from the stack.
    """
    for stack_token in self._stack:
      if (stack_token.overridden_by == token_info and
          not stack_token.is_permanent_override):
        stack_token.overridden_by = None

  def _PopTransient(self):
    """Pops all transient tokens - i.e. not blocks, literals, or parens."""
    while self._stack and self._stack[-1].is_transient:
      self._Pop()
