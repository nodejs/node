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

"""Interface for a linter error handler.

Error handlers aggregate a set of errors from multiple files and can optionally
perform some action based on the reported errors, for example, logging the error
or automatically fixing it.
"""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')


class ErrorHandler(object):
  """Error handler interface."""

  def __init__(self):
    if self.__class__ == ErrorHandler:
      raise NotImplementedError('class ErrorHandler is abstract')

  def HandleFile(self, filename, first_token):
    """Notifies this ErrorHandler that subsequent errors are in filename.

    Args:
      filename: The file being linted.
      first_token: The first token of the file.
    """

  def HandleError(self, error):
    """Append the error to the list.

    Args:
      error: The error object
    """

  def FinishFile(self):
    """Finishes handling the current file.

    Should be called after all errors in a file have been handled.
    """

  def GetErrors(self):
    """Returns the accumulated errors.

    Returns:
      A sequence of errors.
    """
