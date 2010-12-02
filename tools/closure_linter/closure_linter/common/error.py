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

"""Error object commonly used in linters."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')


class Error(object):
  """Object representing a style error."""

  def __init__(self, code, message, token, position, fix_data):
    """Initialize the error object.

    Args:
      code: The numeric error code.
      message: The error message string.
      token: The tokens.Token where the error occurred.
      position: The position of the error within the token.
      fix_data: Data to be used in autofixing.  Codes with fix_data are:
          GOOG_REQUIRES_NOT_ALPHABETIZED - List of string value tokens that are
          class names in goog.requires calls.
    """
    self.code = code
    self.message = message
    self.token = token
    self.position = position
    if token:
      self.start_index = token.start_index
    else:
      self.start_index = 0
    self.fix_data = fix_data
    if self.position:
      self.start_index += self.position.start

  def Compare(a, b):
    """Compare two error objects, by source code order.

    Args:
      a: First error object.
      b: Second error object.

    Returns:
      A Negative/0/Positive number when a is before/the same as/after b.
    """
    line_diff = a.token.line_number - b.token.line_number
    if line_diff:
      return line_diff

    return a.start_index - b.start_index
  Compare = staticmethod(Compare)
