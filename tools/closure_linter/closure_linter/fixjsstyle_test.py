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

"""Medium tests for the gpylint auto-fixer."""

__author__ = 'robbyw@google.com (Robby Walker)'

import StringIO

import gflags as flags
import unittest as googletest
from closure_linter import checker
from closure_linter import error_fixer

_RESOURCE_PREFIX = 'closure_linter/testdata'

flags.FLAGS.strict = True
flags.FLAGS.limited_doc_files = ('dummy.js', 'externs.js')
flags.FLAGS.closurized_namespaces = ('goog', 'dummy')

class FixJsStyleTest(googletest.TestCase):
  """Test case to for gjslint auto-fixing."""

  def testFixJsStyle(self):
    input_filename = None
    try:
      input_filename = '%s/fixjsstyle.in.js' % (_RESOURCE_PREFIX)

      golden_filename = '%s/fixjsstyle.out.js' % (_RESOURCE_PREFIX)
    except IOError, ex:
      raise IOError('Could not find testdata resource for %s: %s' %
                    (self._filename, ex))

    # Autofix the file, sending output to a fake file.
    actual = StringIO.StringIO()
    style_checker = checker.JavaScriptStyleChecker(
        error_fixer.ErrorFixer(actual))
    style_checker.Check(input_filename)

    # Now compare the files.
    actual.seek(0)
    expected = open(golden_filename, 'r')

    self.assertEqual(actual.readlines(), expected.readlines())


if __name__ == '__main__':
  googletest.main()
