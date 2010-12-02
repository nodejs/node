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

"""Classes to represent tokens and positions within them."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')


class TokenType(object):
  """Token types common to all languages."""
  NORMAL = 'normal'
  WHITESPACE = 'whitespace'
  BLANK_LINE = 'blank line'


class Token(object):
  """Token class for intelligent text splitting.

  The token class represents a string of characters and an identifying type.

  Attributes:
    type: The type of token.
    string: The characters the token comprises.
    length: The length of the token.
    line: The text of the line the token is found in.
    line_number: The number of the line the token is found in.
    values: Dictionary of values returned from the tokens regex match.
    previous: The token before this one.
    next: The token after this one.
    start_index: The character index in the line where this token starts.
    attached_object: Object containing more information about this token.
    metadata: Object containing metadata about this token.  Must be added by
        a separate metadata pass.
  """

  def __init__(self, string, token_type, line, line_number, values=None):
    """Creates a new Token object.

    Args:
      string: The string of input the token contains.
      token_type: The type of token.
      line: The text of the line this token is in.
      line_number: The line number of the token.
      values: A dict of named values within the token.  For instance, a
        function declaration may have a value called 'name' which captures the
        name of the function.
    """
    self.type = token_type
    self.string = string
    self.length = len(string)
    self.line = line
    self.line_number = line_number
    self.values = values

    # These parts can only be computed when the file is fully tokenized
    self.previous = None
    self.next = None
    self.start_index = None

    # This part is set in statetracker.py
    # TODO(robbyw): Wrap this in to metadata
    self.attached_object = None

    # This part is set in *metadatapass.py
    self.metadata = None

  def IsFirstInLine(self):
    """Tests if this token is the first token in its line.

    Returns:
      Whether the token is the first token in its line.
    """
    return not self.previous or self.previous.line_number != self.line_number

  def IsLastInLine(self):
    """Tests if this token is the last token in its line.

    Returns:
      Whether the token is the last token in its line.
    """
    return not self.next or self.next.line_number != self.line_number

  def IsType(self, token_type):
    """Tests if this token is of the given type.

    Args:
      token_type: The type to test for.

    Returns:
      True if the type of this token matches the type passed in.
    """
    return self.type == token_type

  def IsAnyType(self, *token_types):
    """Tests if this token is any of the given types.

    Args:
      token_types: The types to check.  Also accepts a single array.

    Returns:
      True if the type of this token is any of the types passed in.
    """
    if not isinstance(token_types[0], basestring):
      return self.type in token_types[0]
    else:
      return self.type in token_types

  def __repr__(self):
    return '<Token: %s, "%s", %r, %d, %r>' % (self.type, self.string,
                                              self.values, self.line_number,
                                              self.metadata)
