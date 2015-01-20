#!/usr/bin/env python
#
# Copyright 2007 The Closure Linter Authors. All Rights Reserved.
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

"""Token utility functions."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

import copy
import StringIO

from closure_linter.common import tokens
from closure_linter.javascripttokens import JavaScriptToken
from closure_linter.javascripttokens import JavaScriptTokenType

# Shorthand
Type = tokens.TokenType


def GetFirstTokenInSameLine(token):
  """Returns the first token in the same line as token.

  Args:
    token: Any token in the line.

  Returns:
    The first token in the same line as token.
  """
  while not token.IsFirstInLine():
    token = token.previous
  return token


def GetFirstTokenInPreviousLine(token):
  """Returns the first token in the previous line as token.

  Args:
    token: Any token in the line.

  Returns:
    The first token in the previous line as token, or None if token is on the
    first line.
  """
  first_in_line = GetFirstTokenInSameLine(token)
  if first_in_line.previous:
    return GetFirstTokenInSameLine(first_in_line.previous)

  return None


def GetLastTokenInSameLine(token):
  """Returns the last token in the same line as token.

  Args:
    token: Any token in the line.

  Returns:
    The last token in the same line as token.
  """
  while not token.IsLastInLine():
    token = token.next
  return token


def GetAllTokensInSameLine(token):
  """Returns all tokens in the same line as the given token.

  Args:
    token: Any token in the line.

  Returns:
    All tokens on the same line as the given token.
  """
  first_token = GetFirstTokenInSameLine(token)
  last_token = GetLastTokenInSameLine(token)

  tokens_in_line = []
  while first_token != last_token:
    tokens_in_line.append(first_token)
    first_token = first_token.next
  tokens_in_line.append(last_token)

  return tokens_in_line


def CustomSearch(start_token, func, end_func=None, distance=None,
                 reverse=False):
  """Returns the first token where func is True within distance of this token.

  Args:
    start_token: The token to start searching from
    func: The function to call to test a token for applicability
    end_func: The function to call to test a token to determine whether to abort
          the search.
    distance: The number of tokens to look through before failing search.  Must
        be positive.  If unspecified, will search until the end of the token
        chain
    reverse: When true, search the tokens before this one instead of the tokens
        after it

  Returns:
    The first token matching func within distance of this token, or None if no
    such token is found.
  """
  token = start_token
  if reverse:
    while token and (distance is None or distance > 0):
      previous = token.previous
      if previous:
        if func(previous):
          return previous
        if end_func and end_func(previous):
          return None

      token = previous
      if distance is not None:
        distance -= 1

  else:
    while token and (distance is None or distance > 0):
      next_token = token.next
      if next_token:
        if func(next_token):
          return next_token
        if end_func and end_func(next_token):
          return None

      token = next_token
      if distance is not None:
        distance -= 1

  return None


def Search(start_token, token_types, distance=None, reverse=False):
  """Returns the first token of type in token_types within distance.

  Args:
    start_token: The token to start searching from
    token_types: The allowable types of the token being searched for
    distance: The number of tokens to look through before failing search.  Must
        be positive.  If unspecified, will search until the end of the token
        chain
    reverse: When true, search the tokens before this one instead of the tokens
        after it

  Returns:
    The first token of any type in token_types within distance of this token, or
    None if no such token is found.
  """
  return CustomSearch(start_token, lambda token: token.IsAnyType(token_types),
                      None, distance, reverse)


def SearchExcept(start_token, token_types, distance=None, reverse=False):
  """Returns the first token not of any type in token_types within distance.

  Args:
    start_token: The token to start searching from
    token_types: The unallowable types of the token being searched for
    distance: The number of tokens to look through before failing search.  Must
        be positive.  If unspecified, will search until the end of the token
        chain
    reverse: When true, search the tokens before this one instead of the tokens
        after it

  Returns:
    The first token of any type in token_types within distance of this token, or
    None if no such token is found.
  """
  return CustomSearch(start_token,
                      lambda token: not token.IsAnyType(token_types),
                      None, distance, reverse)


def SearchUntil(start_token, token_types, end_types, distance=None,
                reverse=False):
  """Returns the first token of type in token_types before a token of end_type.

  Args:
    start_token: The token to start searching from.
    token_types: The allowable types of the token being searched for.
    end_types: Types of tokens to abort search if we find.
    distance: The number of tokens to look through before failing search.  Must
        be positive.  If unspecified, will search until the end of the token
        chain
    reverse: When true, search the tokens before this one instead of the tokens
        after it

  Returns:
    The first token of any type in token_types within distance of this token
    before any tokens of type in end_type, or None if no such token is found.
  """
  return CustomSearch(start_token, lambda token: token.IsAnyType(token_types),
                      lambda token: token.IsAnyType(end_types),
                      distance, reverse)


def DeleteToken(token):
  """Deletes the given token from the linked list.

  Args:
    token: The token to delete
  """
  # When deleting a token, we do not update the deleted token itself to make
  # sure the previous and next pointers are still pointing to tokens which are
  # not deleted.  Also it is very hard to keep track of all previously deleted
  # tokens to update them when their pointers become invalid.  So we add this
  # flag that any token linked list iteration logic can skip deleted node safely
  # when its current token is deleted.
  token.is_deleted = True
  if token.previous:
    token.previous.next = token.next

  if token.next:
    token.next.previous = token.previous

    following_token = token.next
    while following_token and following_token.metadata.last_code == token:
      following_token.metadata.last_code = token.metadata.last_code
      following_token = following_token.next


def DeleteTokens(token, token_count):
  """Deletes the given number of tokens starting with the given token.

  Args:
    token: The token to start deleting at.
    token_count: The total number of tokens to delete.
  """
  for i in xrange(1, token_count):
    DeleteToken(token.next)
  DeleteToken(token)


def InsertTokenBefore(new_token, token):
  """Insert new_token before token.

  Args:
    new_token: A token to be added to the stream
    token: A token already in the stream
  """
  new_token.next = token
  new_token.previous = token.previous

  new_token.metadata = copy.copy(token.metadata)

  if new_token.IsCode():
    old_last_code = token.metadata.last_code
    following_token = token
    while (following_token and
           following_token.metadata.last_code == old_last_code):
      following_token.metadata.last_code = new_token
      following_token = following_token.next

  token.previous = new_token
  if new_token.previous:
    new_token.previous.next = new_token

  if new_token.start_index is None:
    if new_token.line_number == token.line_number:
      new_token.start_index = token.start_index
    else:
      previous_token = new_token.previous
      if previous_token:
        new_token.start_index = (previous_token.start_index +
                                 len(previous_token.string))
      else:
        new_token.start_index = 0

    iterator = new_token.next
    while iterator and iterator.line_number == new_token.line_number:
      iterator.start_index += len(new_token.string)
      iterator = iterator.next


def InsertTokenAfter(new_token, token):
  """Insert new_token after token.

  Args:
    new_token: A token to be added to the stream
    token: A token already in the stream
  """
  new_token.previous = token
  new_token.next = token.next

  new_token.metadata = copy.copy(token.metadata)

  if token.IsCode():
    new_token.metadata.last_code = token

  if new_token.IsCode():
    following_token = token.next
    while following_token and following_token.metadata.last_code == token:
      following_token.metadata.last_code = new_token
      following_token = following_token.next

  token.next = new_token
  if new_token.next:
    new_token.next.previous = new_token

  if new_token.start_index is None:
    if new_token.line_number == token.line_number:
      new_token.start_index = token.start_index + len(token.string)
    else:
      new_token.start_index = 0

    iterator = new_token.next
    while iterator and iterator.line_number == new_token.line_number:
      iterator.start_index += len(new_token.string)
      iterator = iterator.next


def InsertTokensAfter(new_tokens, token):
  """Insert multiple tokens after token.

  Args:
    new_tokens: An array of tokens to be added to the stream
    token: A token already in the stream
  """
  # TODO(user): It would be nicer to have InsertTokenAfter defer to here
  # instead of vice-versa.
  current_token = token
  for new_token in new_tokens:
    InsertTokenAfter(new_token, current_token)
    current_token = new_token


def InsertSpaceTokenAfter(token):
  """Inserts a space token after the given token.

  Args:
    token: The token to insert a space token after

  Returns:
    A single space token
  """
  space_token = JavaScriptToken(' ', Type.WHITESPACE, token.line,
                                token.line_number)
  InsertTokenAfter(space_token, token)


def InsertBlankLineAfter(token):
  """Inserts a blank line after the given token.

  Args:
    token: The token to insert a blank line after

  Returns:
    A single space token
  """
  blank_token = JavaScriptToken('', Type.BLANK_LINE, '',
                                token.line_number + 1)
  InsertLineAfter(token, [blank_token])


def InsertLineAfter(token, new_tokens):
  """Inserts a new line consisting of new_tokens after the given token.

  Args:
    token: The token to insert after.
    new_tokens: The tokens that will make up the new line.
  """
  insert_location = token
  for new_token in new_tokens:
    InsertTokenAfter(new_token, insert_location)
    insert_location = new_token

  # Update all subsequent line numbers.
  next_token = new_tokens[-1].next
  while next_token:
    next_token.line_number += 1
    next_token = next_token.next


def SplitToken(token, position):
  """Splits the token into two tokens at position.

  Args:
    token: The token to split
    position: The position to split at. Will be the beginning of second token.

  Returns:
    The new second token.
  """
  new_string = token.string[position:]
  token.string = token.string[:position]

  new_token = JavaScriptToken(new_string, token.type, token.line,
                              token.line_number)
  InsertTokenAfter(new_token, token)

  return new_token


def Compare(token1, token2):
  """Compares two tokens and determines their relative order.

  Args:
    token1: The first token to compare.
    token2: The second token to compare.

  Returns:
    A negative integer, zero, or a positive integer as the first token is
    before, equal, or after the second in the token stream.
  """
  if token2.line_number != token1.line_number:
    return token1.line_number - token2.line_number
  else:
    return token1.start_index - token2.start_index


def GoogScopeOrNoneFromStartBlock(token):
  """Determines if the given START_BLOCK is part of a goog.scope statement.

  Args:
    token: A token of type START_BLOCK.

  Returns:
    The goog.scope function call token, or None if such call doesn't exist.
  """
  if token.type != JavaScriptTokenType.START_BLOCK:
    return None

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


def GetTokenRange(start_token, end_token):
  """Returns a list of tokens between the two given, inclusive.

  Args:
    start_token: Start token in the range.
    end_token: End token in the range.

  Returns:
    A list of tokens, in order, from start_token to end_token (including start
    and end).  Returns none if the tokens do not describe a valid range.
  """

  token_range = []
  token = start_token

  while token:
    token_range.append(token)

    if token == end_token:
      return token_range

    token = token.next


def TokensToString(token_iterable):
  """Convert a number of tokens into a string.

  Newlines will be inserted whenever the line_number of two neighboring
  strings differ.

  Args:
    token_iterable: The tokens to turn to a string.

  Returns:
    A string representation of the given tokens.
  """

  buf = StringIO.StringIO()
  token_list = list(token_iterable)
  if not token_list:
    return ''

  line_number = token_list[0].line_number

  for token in token_list:

    while line_number < token.line_number:
      line_number += 1
      buf.write('\n')

    if line_number > token.line_number:
      line_number = token.line_number
      buf.write('\n')

    buf.write(token.string)

  return buf.getvalue()


def GetPreviousCodeToken(token):
  """Returns the code token before the specified token.

  Args:
    token: A token.

  Returns:
    The code token before the specified token or None if no such token
    exists.
  """

  return CustomSearch(
      token,
      lambda t: t and t.type not in JavaScriptTokenType.NON_CODE_TYPES,
      reverse=True)


def GetNextCodeToken(token):
  """Returns the next code token after the specified token.

  Args:
    token: A token.

  Returns:
    The next code token after the specified token or None if no such token
    exists.
  """

  return CustomSearch(
      token,
      lambda t: t and t.type not in JavaScriptTokenType.NON_CODE_TYPES,
      reverse=False)


def GetIdentifierStart(token):
  """Returns the first token in an identifier.

  Given a token which is part of an identifier, returns the token at the start
  of the identifier.

  Args:
    token: A token which is part of an identifier.

  Returns:
    The token at the start of the identifier or None if the identifier was not
    of the form 'a.b.c' (e.g. "['a']['b'].c").
  """

  start_token = token
  previous_code_token = GetPreviousCodeToken(token)

  while (previous_code_token and (
      previous_code_token.IsType(JavaScriptTokenType.IDENTIFIER) or
      IsDot(previous_code_token))):
    start_token = previous_code_token
    previous_code_token = GetPreviousCodeToken(previous_code_token)

  if IsDot(start_token):
    return None

  return start_token


def GetIdentifierForToken(token):
  """Get the symbol specified by a token.

  Given a token, this function additionally concatenates any parts of an
  identifying symbol being identified that are split by whitespace or a
  newline.

  The function will return None if the token is not the first token of an
  identifier.

  Args:
    token: The first token of a symbol.

  Returns:
    The whole symbol, as a string.
  """

  # Search backward to determine if this token is the first token of the
  # identifier. If it is not the first token, return None to signal that this
  # token should be ignored.
  prev_token = token.previous
  while prev_token:
    if (prev_token.IsType(JavaScriptTokenType.IDENTIFIER) or
        IsDot(prev_token)):
      return None

    if (prev_token.IsType(tokens.TokenType.WHITESPACE) or
        prev_token.IsAnyType(JavaScriptTokenType.COMMENT_TYPES)):
      prev_token = prev_token.previous
    else:
      break

  # A "function foo()" declaration.
  if token.type is JavaScriptTokenType.FUNCTION_NAME:
    return token.string

  # A "var foo" declaration (if the previous token is 'var')
  previous_code_token = GetPreviousCodeToken(token)

  if previous_code_token and previous_code_token.IsKeyword('var'):
    return token.string

  # Otherwise, this is potentially a namespaced (goog.foo.bar) identifier that
  # could span multiple lines or be broken up by whitespace.  We need
  # to concatenate.
  identifier_types = set([
      JavaScriptTokenType.IDENTIFIER,
      JavaScriptTokenType.SIMPLE_LVALUE
      ])

  assert token.type in identifier_types

  # Start with the first token
  symbol_tokens = [token]

  if token.next:
    for t in token.next:
      last_symbol_token = symbol_tokens[-1]

      # A dot is part of the previous symbol.
      if IsDot(t):
        symbol_tokens.append(t)
        continue

      # An identifier is part of the previous symbol if the previous one was a
      # dot.
      if t.type in identifier_types:
        if IsDot(last_symbol_token):
          symbol_tokens.append(t)
          continue
        else:
          break

      # Skip any whitespace
      if t.type in JavaScriptTokenType.NON_CODE_TYPES:
        continue

      # This is the end of the identifier. Stop iterating.
      break

  if symbol_tokens:
    return ''.join([t.string for t in symbol_tokens])


def GetStringAfterToken(token):
  """Get string after token.

  Args:
    token: Search will be done after this token.

  Returns:
    String if found after token else None (empty string will also
    return None).

  Search until end of string as in case of empty string Type.STRING_TEXT is not
  present/found and don't want to return next string.
  E.g.
  a = '';
  b = 'test';
  When searching for string after 'a' if search is not limited by end of string
  then it will return 'test' which is not desirable as there is a empty string
  before that.

  This will return None for cases where string is empty or no string found
  as in both cases there is no Type.STRING_TEXT.
  """
  string_token = SearchUntil(token, JavaScriptTokenType.STRING_TEXT,
                             [JavaScriptTokenType.SINGLE_QUOTE_STRING_END,
                              JavaScriptTokenType.DOUBLE_QUOTE_STRING_END])
  if string_token:
    return string_token.string
  else:
    return None


def IsDot(token):
  """Whether the token represents a "dot" operator (foo.bar)."""
  return token.type is JavaScriptTokenType.OPERATOR and token.string == '.'


def IsIdentifierOrDot(token):
  """Whether the token is either an identifier or a '.'."""
  return (token.type in [JavaScriptTokenType.IDENTIFIER,
                         JavaScriptTokenType.SIMPLE_LVALUE] or
          IsDot(token))
