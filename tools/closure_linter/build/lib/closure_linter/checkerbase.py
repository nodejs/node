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

"""Base classes for writing checkers that operate on tokens."""

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)',
              'jacobr@google.com (Jacob Richman)')

from closure_linter import errorrules
from closure_linter.common import error


class LintRulesBase(object):
  """Base class for all classes defining the lint rules for a language."""

  def __init__(self):
    self.__checker = None

  def Initialize(self, checker, limited_doc_checks, is_html):
    """Initializes to prepare to check a file.

    Args:
      checker: Class to report errors to.
      limited_doc_checks: Whether doc checking is relaxed for this file.
      is_html: Whether the file is an HTML file with extracted contents.
    """
    self.__checker = checker
    self._limited_doc_checks = limited_doc_checks
    self._is_html = is_html

  def _HandleError(self, code, message, token, position=None,
                   fix_data=None):
    """Call the HandleError function for the checker we are associated with."""
    if errorrules.ShouldReportError(code):
      self.__checker.HandleError(code, message, token, position, fix_data)

  def _SetLimitedDocChecks(self, limited_doc_checks):
    """Sets whether doc checking is relaxed for this file.

    Args:
      limited_doc_checks: Whether doc checking is relaxed for this file.
    """
    self._limited_doc_checks = limited_doc_checks

  def CheckToken(self, token, parser_state):
    """Checks a token, given the current parser_state, for warnings and errors.

    Args:
      token: The current token under consideration.
      parser_state: Object that indicates the parser state in the page.

    Raises:
      TypeError: If not overridden.
    """
    raise TypeError('Abstract method CheckToken not implemented')

  def Finalize(self, parser_state):
    """Perform all checks that need to occur after all lines are processed.

    Args:
      parser_state: State of the parser after parsing all tokens

    Raises:
      TypeError: If not overridden.
    """
    raise TypeError('Abstract method Finalize not implemented')


class CheckerBase(object):
  """This class handles checking a LintRules object against a file."""

  def __init__(self, error_handler, lint_rules, state_tracker):
    """Initialize a checker object.

    Args:
      error_handler: Object that handles errors.
      lint_rules: LintRules object defining lint errors given a token
        and state_tracker object.
      state_tracker: Object that tracks the current state in the token stream.

    """
    self._error_handler = error_handler
    self._lint_rules = lint_rules
    self._state_tracker = state_tracker

    self._has_errors = False

  def HandleError(self, code, message, token, position=None,
                  fix_data=None):
    """Prints out the given error message including a line number.

    Args:
      code: The error code.
      message: The error to print.
      token: The token where the error occurred, or None if it was a file-wide
          issue.
      position: The position of the error, defaults to None.
      fix_data: Metadata used for fixing the error.
    """
    self._has_errors = True
    self._error_handler.HandleError(
        error.Error(code, message, token, position, fix_data))

  def HasErrors(self):
    """Returns true if the style checker has found any errors.

    Returns:
      True if the style checker has found any errors.
    """
    return self._has_errors

  def Check(self, start_token, limited_doc_checks=False, is_html=False,
            stop_token=None):
    """Checks a token stream, reporting errors to the error reporter.

    Args:
      start_token: First token in token stream.
      limited_doc_checks: Whether doc checking is relaxed for this file.
      is_html: Whether the file being checked is an HTML file with extracted
          contents.
      stop_token: If given, check should stop at this token.
    """

    self._lint_rules.Initialize(self, limited_doc_checks, is_html)
    self._ExecutePass(start_token, self._LintPass, stop_token=stop_token)
    self._lint_rules.Finalize(self._state_tracker)

  def _LintPass(self, token):
    """Checks an individual token for lint warnings/errors.

    Used to encapsulate the logic needed to check an individual token so that it
    can be passed to _ExecutePass.

    Args:
      token: The token to check.
    """
    self._lint_rules.CheckToken(token, self._state_tracker)

  def _ExecutePass(self, token, pass_function, stop_token=None):
    """Calls the given function for every token in the given token stream.

    As each token is passed to the given function, state is kept up to date and,
    depending on the error_trace flag, errors are either caught and reported, or
    allowed to bubble up so developers can see the full stack trace. If a parse
    error is specified, the pass will proceed as normal until the token causing
    the parse error is reached.

    Args:
      token: The first token in the token stream.
      pass_function: The function to call for each token in the token stream.
      stop_token: The last token to check (if given).

    Raises:
      Exception: If any error occurred while calling the given function.
    """

    self._state_tracker.Reset()
    while token:
      # When we are looking at a token and decided to delete the whole line, we
      # will delete all of them in the "HandleToken()" below.  So the current
      # token and subsequent ones may already be deleted here.  The way we
      # delete a token does not wipe out the previous and next pointers of the
      # deleted token.  So we need to check the token itself to make sure it is
      # not deleted.
      if not token.is_deleted:
        # End the pass at the stop token
        if stop_token and token is stop_token:
          return

        self._state_tracker.HandleToken(
            token, self._state_tracker.GetLastNonSpaceToken())
        pass_function(token)
        self._state_tracker.HandleAfterToken(token)

      token = token.next
