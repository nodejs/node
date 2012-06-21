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

from closure_linter import javascripttokens
from closure_linter.common import tokens

# Shorthand
JavaScriptToken = javascripttokens.JavaScriptToken
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
