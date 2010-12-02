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

"""Regular expression based JavaScript matcher classes."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

from closure_linter.common import position
from closure_linter.common import tokens

# Shorthand
Token = tokens.Token
Position = position.Position


class Matcher(object):
  """A token matcher.

  Specifies a pattern to match, the type of token it represents, what mode the
  token changes to, and what mode the token applies to.

  Modes allow more advanced grammars to be incorporated, and are also necessary
  to tokenize line by line.  We can have different patterns apply to different
  modes - i.e. looking for documentation while in comment mode.

  Attributes:
    regex: The regular expression representing this matcher.
    type: The type of token indicated by a successful match.
    result_mode: The mode to move to after a successful match.
  """

  def __init__(self, regex, token_type, result_mode=None, line_start=False):
    """Create a new matcher template.

    Args:
      regex: The regular expression to match.
      token_type: The type of token a successful match indicates.
      result_mode: What mode to change to after a successful match.  Defaults to
        None, which means to not change the current mode.
      line_start: Whether this matcher should only match string at the start
        of a line.
    """
    self.regex = regex
    self.type = token_type
    self.result_mode = result_mode
    self.line_start = line_start
