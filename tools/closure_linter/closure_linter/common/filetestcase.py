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

"""Test case that runs a checker on a file, matching errors against annotations.

Runs the given checker on the given file, accumulating all errors.  The list
of errors is then matched against those annotated in the file.  Based heavily
on devtools/javascript/gpylint/full_test.py.
"""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

import re

import unittest as googletest
from closure_linter.common import erroraccumulator


class AnnotatedFileTestCase(googletest.TestCase):
  """Test case to run a linter against a single file."""

  # Matches an all caps letters + underscores error identifer
  _MESSAGE = {'msg': '[A-Z][A-Z_]+'}
  # Matches a //, followed by an optional line number with a +/-, followed by a
  # list of message IDs. Used to extract expected messages from testdata files.
  # TODO(robbyw): Generalize to use different commenting patterns.
  _EXPECTED_RE = re.compile(r'\s*//\s*(?:(?P<line>[+-]?[0-9]+):)?'
                            r'\s*(?P<msgs>%(msg)s(?:,\s*%(msg)s)*)' % _MESSAGE)

  def __init__(self, filename, runner, converter):
    """Create a single file lint test case.

    Args:
      filename: Filename to test.
      runner: Object implementing the LintRunner interface that lints a file.
      converter: Function taking an error string and returning an error code.
    """

    googletest.TestCase.__init__(self, 'runTest')
    self._filename = filename
    self._messages = []
    self._runner = runner
    self._converter = converter

  def shortDescription(self):
    """Provides a description for the test."""
    return 'Run linter on %s' % self._filename

  def runTest(self):
    """Runs the test."""
    try:
      filename = self._filename
      stream = open(filename)
    except IOError, ex:
      raise IOError('Could not find testdata resource for %s: %s' %
                    (self._filename, ex))

    expected = self._GetExpectedMessages(stream)
    got = self._ProcessFileAndGetMessages(filename)
    self.assertEqual(expected, got)

  def _GetExpectedMessages(self, stream):
    """Parse a file and get a sorted list of expected messages."""
    messages = []
    for i, line in enumerate(stream):
      match = self._EXPECTED_RE.search(line)
      if match:
        line = match.group('line')
        msg_ids = match.group('msgs')
        if line is None:
          line = i + 1
        elif line.startswith('+') or line.startswith('-'):
          line = i + 1 + int(line)
        else:
          line = int(line)
        for msg_id in msg_ids.split(','):
          # Ignore a spurious message from the license preamble.
          if msg_id != 'WITHOUT':
            messages.append((line, self._converter(msg_id.strip())))
    stream.seek(0)
    messages.sort()
    return messages

  def _ProcessFileAndGetMessages(self, filename):
    """Trap gpylint's output parse it to get messages added."""
    errors = erroraccumulator.ErrorAccumulator()
    self._runner.Run([filename], errors)

    errors = errors.GetErrors()

    # Convert to expected tuple format.
    error_msgs = [(error.token.line_number, error.code) for error in errors]
    error_msgs.sort()
    return error_msgs
