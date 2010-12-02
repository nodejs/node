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

"""Interface for a lint running wrapper."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')


class LintRunner(object):
  """Interface for a lint running wrapper."""

  def __init__(self):
    if self.__class__ == LintRunner:
      raise NotImplementedError('class LintRunner is abstract')

  def Run(self, filenames, error_handler):
    """Run a linter on the given filenames.

    Args:
      filenames: The filenames to check
      error_handler: An ErrorHandler object

    Returns:
      The error handler, which may have been used to collect error info.
    """
