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

"""Full regression-type (Medium) tests for gjslint.

Tests every error that can be thrown by gjslint.  Based heavily on
devtools/javascript/gpylint/full_test.py
"""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

import re
import os
import sys
import unittest

import gflags as flags
import unittest as googletest

from closure_linter import checker
from closure_linter import errors
from closure_linter.common import filetestcase

_RESOURCE_PREFIX = 'closure_linter/testdata'

flags.FLAGS.strict = True
flags.FLAGS.custom_jsdoc_tags = ('customtag', 'requires')
flags.FLAGS.closurized_namespaces = ('goog', 'dummy')
flags.FLAGS.limited_doc_files = ('externs.js', 'dummy.js')

# List of files under testdata to test.
# We need to list files explicitly since pyglib can't list directories.
_TEST_FILES = [
    'all_js_wrapped.js',
    'blank_lines.js',
    'ends_with_block.js',
    'externs.js',
    'html_parse_error.html',
    'indentation.js',
    'interface.js',
    'jsdoc.js',
    'minimal.js',
    'other.js',
    'require_all_caps.js',
    'require_extra.js',
    'require_function.js',
    'require_function_missing.js',
    'require_function_through_both.js',
    'require_function_through_namespace.js',
    'require_interface.js',
    'require_lower_case.js',
    'require_numeric.js',
    'require_provide_ok.js',
    'require_provide_missing.js',
    'simple.html',
    'spaces.js',
    'tokenizer.js',
    'unparseable.js',
    'utf8.html'
    ]


class GJsLintTestSuite(unittest.TestSuite):
  """Test suite to run a GJsLintTest for each of several files.

  If sys.argv[1:] is non-empty, it is interpreted as a list of filenames in
  testdata to test. Otherwise, _TEST_FILES is used.
  """

  def __init__(self, tests=()):
    unittest.TestSuite.__init__(self, tests)

    argv = sys.argv and sys.argv[1:] or []
    if argv:
      test_files = argv
    else:
      test_files = _TEST_FILES
    for test_file in test_files:
      resource_path = os.path.join(_RESOURCE_PREFIX, test_file)
      self.addTest(filetestcase.AnnotatedFileTestCase(resource_path,
          checker.GJsLintRunner(), errors.ByName))

if __name__ == '__main__':
  # Don't let main parse args; it happens in the TestSuite.
  googletest.main(argv=sys.argv[0:1], defaultTest='GJsLintTestSuite')
