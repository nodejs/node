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

"""Regular expression based lexer."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

from closure_linter.common import tokens

# Shorthand
Type = tokens.TokenType


class Tokenizer(object):
  """General purpose tokenizer.

  Attributes:
    mode: The latest mode of the tokenizer.  This allows patterns to distinguish
        if they are mid-comment, mid-parameter list, etc.
    matchers: Dictionary of modes to sequences of matchers that define the
        patterns to check at any given time.
    default_types: Dictionary of modes to types, defining what type to give
        non-matched text when in the given mode.  Defaults to Type.NORMAL.
  """

  def __init__(self, starting_mode, matchers, default_types):
    """Initialize the tokenizer.

    Args:
      starting_mode: Mode to start in.
      matchers: Dictionary of modes to sequences of matchers that defines the
          patterns to check at any given time.
      default_types: Dictionary of modes to types, defining what type to give
          non-matched text when in the given mode.  Defaults to Type.NORMAL.
    """
    self.__starting_mode = starting_mode
    self.matchers = matchers
    self.default_types = default_types

  def TokenizeFile(self, file):
    """Tokenizes the given file.

    Args:
      file: An iterable that yields one line of the file at a time.

    Returns:
      The first token in the file
    """
    # The current mode.
    self.mode = self.__starting_mode
    # The first token in the stream.
    self.__first_token = None
    # The last token added to the token stream.
    self.__last_token = None
    # The current line number.
    self.__line_number = 0

    for line in file:
      self.__line_number += 1
      self.__TokenizeLine(line)

    return self.__first_token

  def _CreateToken(self, string, token_type, line, line_number, values=None):
    """Creates a new Token object (or subclass).

    Args:
      string: The string of input the token represents.
      token_type: The type of token.
      line: The text of the line this token is in.
      line_number: The line number of the token.
      values: A dict of named values within the token.  For instance, a
        function declaration may have a value called 'name' which captures the
        name of the function.

    Returns:
      The newly created Token object.
    """
    return tokens.Token(string, token_type, line, line_number, values)

  def __TokenizeLine(self, line):
    """Tokenizes the given line.

    Args:
      line: The contents of the line.
    """
    string = line.rstrip('\n\r\f')
    line_number = self.__line_number
    self.__start_index = 0

    if not string:
      self.__AddToken(self._CreateToken('', Type.BLANK_LINE, line, line_number))
      return

    normal_token = ''
    index = 0
    while index < len(string):
      for matcher in self.matchers[self.mode]:
        if matcher.line_start and index > 0:
          continue

        match = matcher.regex.match(string, index)

        if match:
          if normal_token:
            self.__AddToken(
                self.__CreateNormalToken(self.mode, normal_token, line,
                                         line_number))
            normal_token = ''

          # Add the match.
          self.__AddToken(self._CreateToken(match.group(), matcher.type, line,
                                            line_number, match.groupdict()))

          # Change the mode to the correct one for after this match.
          self.mode = matcher.result_mode or self.mode

          # Shorten the string to be matched.
          index = match.end()

          break

      else:
        # If the for loop finishes naturally (i.e. no matches) we just add the
        # first character to the string of consecutive non match characters.
        # These will constitute a NORMAL token.
        if string:
          normal_token += string[index:index + 1]
          index += 1

    if normal_token:
      self.__AddToken(
          self.__CreateNormalToken(self.mode, normal_token, line, line_number))

  def __CreateNormalToken(self, mode, string, line, line_number):
    """Creates a normal token.

    Args:
      mode: The current mode.
      string: The string to tokenize.
      line: The line of text.
      line_number: The line number within the file.

    Returns:
      A Token object, of the default type for the current mode.
    """
    type = Type.NORMAL
    if mode in self.default_types:
      type = self.default_types[mode]
    return self._CreateToken(string, type, line, line_number)

  def __AddToken(self, token):
    """Add the given token to the token stream.

    Args:
      token: The token to add.
    """
    # Store the first token, or point the previous token to this one.
    if not self.__first_token:
      self.__first_token = token
    else:
      self.__last_token.next = token

    # Establish the doubly linked list
    token.previous = self.__last_token
    self.__last_token = token

    # Compute the character indices
    token.start_index = self.__start_index
    self.__start_index += token.length
