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

"""Linter error handler class that prints errors to stdout."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

from closure_linter.common import error
from closure_linter.common import errorhandler

Error = error.Error


# The error message is of the format:
# Line <number>, E:<code>: message
DEFAULT_FORMAT = 1

# The error message is of the format:
# filename:[line number]:message
UNIX_FORMAT = 2


class ErrorPrinter(errorhandler.ErrorHandler):
  """ErrorHandler that prints errors to stdout."""

  def __init__(self, new_errors=None):
    """Initializes this error printer.

    Args:
      new_errors: A sequence of error codes representing recently introduced
        errors, defaults to None.
    """
    # Number of errors
    self._error_count = 0

    # Number of new errors
    self._new_error_count = 0

    # Number of files checked
    self._total_file_count = 0

    # Number of files with errors
    self._error_file_count = 0

    # Dict of file name to number of errors
    self._file_table = {}

    # List of errors for each file
    self._file_errors = None

    # Current file
    self._filename = None

    self._format = DEFAULT_FORMAT

    if new_errors:
      self._new_errors = frozenset(new_errors)
    else:
      self._new_errors = frozenset(set())

  def SetFormat(self, format):
    """Sets the print format of errors.

    Args:
      format: One of {DEFAULT_FORMAT, UNIX_FORMAT}.
    """
    self._format = format

  def HandleFile(self, filename, first_token):
    """Notifies this ErrorPrinter that subsequent errors are in filename.

    Sets the current file name, and sets a flag stating the header for this file
    has not been printed yet.

    Should be called by a linter before a file is style checked.

    Args:
      filename: The name of the file about to be checked.
      first_token: The first token in the file, or None if there was an error
          opening the file
    """
    if self._filename and self._file_table[self._filename]:
      print

    self._filename = filename
    self._file_table[filename] = 0
    self._total_file_count += 1
    self._file_errors = []

  def HandleError(self, error):
    """Prints a formatted error message about the specified error.

    The error message is of the format:
    Error #<code>, line #<number>: message

    Args:
      error: The error object
    """
    self._file_errors.append(error)
    self._file_table[self._filename] += 1
    self._error_count += 1

    if self._new_errors and error.code in self._new_errors:
      self._new_error_count += 1

  def _PrintError(self, error):
    """Prints a formatted error message about the specified error.

    Args:
      error: The error object
    """
    new_error = self._new_errors and error.code in self._new_errors
    if self._format == DEFAULT_FORMAT:
      line = ''
      if error.token:
        line = 'Line %d, ' % error.token.line_number

      code = 'E:%04d' % error.code
      if new_error:
        print '%s%s: (New error) %s' % (line, code, error.message)
      else:
        print '%s%s: %s' % (line, code, error.message)
    else:
      # UNIX format
      filename = self._filename
      line = ''
      if error.token:
        line = '%d' % error.token.line_number

      error_code = '%04d' % error.code
      if new_error:
        error_code = 'New Error ' + error_code
      print '%s:%s:(%s) %s' % (filename, line, error_code, error.message)

  def FinishFile(self):
    """Finishes handling the current file."""
    if self._file_errors:
      self._error_file_count += 1

      if self._format != UNIX_FORMAT:
        print '----- FILE  :  %s -----' % (self._filename)

      self._file_errors.sort(Error.Compare)

      for error in self._file_errors:
        self._PrintError(error)

  def HasErrors(self):
    """Whether this error printer encountered any errors.

    Returns:
        True if the error printer encountered any errors.
    """
    return self._error_count

  def HasNewErrors(self):
    """Whether this error printer encountered any new errors.

    Returns:
        True if the error printer encountered any new errors.
    """
    return self._new_error_count

  def HasOldErrors(self):
    """Whether this error printer encountered any old errors.

    Returns:
        True if the error printer encountered any old errors.
    """
    return self._error_count - self._new_error_count

  def PrintSummary(self):
    """Print a summary of the number of errors and files."""
    if self.HasErrors() or self.HasNewErrors():
      print ('Found %d errors, including %d new errors, in %d files '
             '(%d files OK).' % (
                 self._error_count,
                 self._new_error_count,
                 self._error_file_count,
                 self._total_file_count - self._error_file_count))
    else:
      print '%d files checked, no errors found.' % self._total_file_count

  def PrintFileSummary(self):
    """Print a detailed summary of the number of errors in each file."""
    keys = self._file_table.keys()
    keys.sort()
    for filename in keys:
      print '%s: %d' % (filename, self._file_table[filename])
