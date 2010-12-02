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

"""Classes to represent positions within strings."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')


class Position(object):
  """Object representing a segment of a string.

  Attributes:
    start: The index in to the string where the segment starts.
    length: The length of the string segment.
  """

  def __init__(self, start, length):
    """Initialize the position object.

    Args:
      start: The start index.
      length: The number of characters to include.
    """
    self.start = start
    self.length = length

  def Get(self, string):
    """Returns this range of the given string.

    Args:
      string: The string to slice.

    Returns:
      The string within the range specified by this object.
    """
    return string[self.start:self.start + self.length]

  def Set(self, target, source):
    """Sets this range within the target string to the source string.

    Args:
      target: The target string.
      source: The source string.

    Returns:
      The resulting string
    """
    return target[:self.start] + source + target[self.start + self.length:]

  def AtEnd(string):
    """Create a Position representing the end of the given string.

    Args:
      string: The string to represent the end of.

    Returns:
      The created Position object.
    """
    return Position(len(string), 0)
  AtEnd = staticmethod(AtEnd)

  def IsAtEnd(self, string):
    """Returns whether this position is at the end of the given string.

    Args:
      string: The string to test for the end of.

    Returns:
      Whether this position is at the end of the given string.
    """
    return self.start == len(string) and self.length == 0

  def AtBeginning():
    """Create a Position representing the beginning of any string.

    Returns:
      The created Position object.
    """
    return Position(0, 0)
  AtBeginning = staticmethod(AtBeginning)

  def IsAtBeginning(self):
    """Returns whether this position is at the beginning of any string.

    Returns:
      Whether this position is at the beginning of any string.
    """
    return self.start == 0 and self.length == 0

  def All(string):
    """Create a Position representing the entire string.

    Args:
      string: The string to represent the entirety of.

    Returns:
      The created Position object.
    """
    return Position(0, len(string))
  All = staticmethod(All)

  def Index(index):
    """Returns a Position object for the specified index.

    Args:
      index: The index to select, inclusively.

    Returns:
      The created Position object.
    """
    return Position(index, 1)
  Index = staticmethod(Index)
