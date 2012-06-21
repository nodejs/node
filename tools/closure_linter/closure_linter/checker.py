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
from closure_linter import closurizednamespacesinfo
from closure_linter import ecmametadatapass
from closure_linter import javascriptlintrules
from closure_linter import javascriptstatetracker
from closure_linter.common import lintrunner

flags.DEFINE_list('limited_doc_files', ['dummy.js', 'externs.js'],
                  'List of files with relaxed documentation checks. Will not '
                  'report errors for missing documentation, some missing '
                  'descriptions, or methods whose @return tags don\'t have a '
                  'matching return statement.')
flags.DEFINE_list('closurized_namespaces', '',
                  'Namespace prefixes, used for testing of'
                  'goog.provide/require')
flags.DEFINE_list('ignored_extra_namespaces', '',
                  'Fully qualified namespaces that should be not be reported '
                  'as extra by the linter.')


class JavaScriptStyleChecker(checkerbase.CheckerBase):
  """Checker that applies JavaScriptLintRules."""

  def __init__(self, error_handler):
    """Initialize an JavaScriptStyleChecker object.

    Args:
      error_handler: Error handler to pass all errors to.
    """
    self._namespaces_info = None
    if flags.FLAGS.closurized_namespaces:
      self._namespaces_info = (
          closurizednamespacesinfo.ClosurizedNamespacesInfo(
              flags.FLAGS.closurized_namespaces,
              flags.FLAGS.ignored_extra_namespaces))

    checkerbase.CheckerBase.__init__(
        self,
        error_handler=error_handler,
        lint_rules=javascriptlintrules.JavaScriptLintRules(
            self._namespaces_info),
        state_tracker=javascriptstatetracker.JavaScriptStateTracker(),
        metadata_pass=ecmametadatapass.EcmaMetaDataPass(),
        limited_doc_files=flags.FLAGS.limited_doc_files)

  def _CheckTokens(self, token, parse_error, debug_tokens):
    """Checks a token stream for lint warnings/errors.

    Adds a separate pass for computing dependency information based on
    goog.require and goog.provide statements prior to the main linting pass.

    Args:
      token: The first token in the token stream.
      parse_error: A ParseError if any errors occurred.
      debug_tokens: Whether every token should be printed as it is encountered
          during the pass.

    Returns:
      A boolean indicating whether the full token stream could be checked or if
      checking failed prematurely.
    """
    # To maximize the amount of errors that get reported before a parse error
    # is displayed, don't run the dependency pass if a parse error exists.
    if self._namespaces_info and not parse_error:
      self._namespaces_info.Reset()
      result = (self._ExecutePass(token, self._DependencyPass) and
                self._ExecutePass(token, self._LintPass,
                                  debug_tokens=debug_tokens))
    else:
      result = self._ExecutePass(token, self._LintPass, parse_error,
                                 debug_tokens)

    if not result:
      return False

    self._lint_rules.Finalize(self._state_tracker, self._tokenizer.mode)

    self._error_handler.FinishFile()
    return True

  def _DependencyPass(self, token):
    """Processes an invidual token for dependency information.

    Used to encapsulate the logic needed to process an individual token so that
    it can be passed to _ExecutePass.

    Args:
      token: The token to process.
    """
    self._namespaces_info.ProcessToken(token, self._state_tracker)


class GJsLintRunner(lintrunner.LintRunner):
  """Wrapper class to run GJsLint."""

  def Run(self, filenames, error_handler):
    """Run GJsLint on the given filenames.

    Args:
      filenames: The filenames to check
      error_handler: An ErrorHandler object.
    """
    checker = JavaScriptStyleChecker(error_handler)

    # Check the list of files.
    for filename in filenames:
      checker.Check(filename)
