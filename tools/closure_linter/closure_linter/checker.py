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

"""Core methods for checking JS files for common style guide violations."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

import gflags as flags

from closure_linter import checkerbase
from closure_linter import ecmametadatapass
from closure_linter import errors
from closure_linter import javascriptlintrules
from closure_linter import javascriptstatetracker
from closure_linter.common import errorprinter
from closure_linter.common import lintrunner

flags.DEFINE_list('limited_doc_files', ['dummy.js', 'externs.js'],
                  'List of files with relaxed documentation checks. Will not '
                  'report errors for missing documentation, some missing '
                  'descriptions, or methods whose @return tags don\'t have a '
                  'matching return statement.')


class JavaScriptStyleChecker(checkerbase.CheckerBase):
  """Checker that applies JavaScriptLintRules."""

  def __init__(self, error_handler):
    """Initialize an JavaScriptStyleChecker object.

    Args:
      error_handler: Error handler to pass all errors to
    """
    checkerbase.CheckerBase.__init__(
        self,
        error_handler=error_handler,
        lint_rules=javascriptlintrules.JavaScriptLintRules(),
        state_tracker=javascriptstatetracker.JavaScriptStateTracker(
            closurized_namespaces=flags.FLAGS.closurized_namespaces),
        metadata_pass=ecmametadatapass.EcmaMetaDataPass(),
        limited_doc_files=flags.FLAGS.limited_doc_files)


class GJsLintRunner(lintrunner.LintRunner):
  """Wrapper class to run GJsLint."""

  def Run(self, filenames, error_handler=None):
    """Run GJsLint on the given filenames.

    Args:
      filenames: The filenames to check
      error_handler: An optional ErrorHandler object, an ErrorPrinter is used if
        none is specified.

    Returns:
      error_count, file_count: The number of errors and the number of files that
          contain errors.
    """
    if not error_handler:
      error_handler = errorprinter.ErrorPrinter(errors.NEW_ERRORS)

    checker = JavaScriptStyleChecker(error_handler)

    # Check the list of files.
    for filename in filenames:
      checker.Check(filename)

    return error_handler
