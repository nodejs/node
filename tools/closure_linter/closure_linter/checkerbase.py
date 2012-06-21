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

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)',
              'jacobr@google.com (Jacob Richman)')

import StringIO
import traceback

import gflags as flags
from closure_linter import ecmametadatapass
from closure_linter import errorrules
from closure_linter import errors
from closure_linter import javascripttokenizer
from closure_linter.common import error
from closure_linter.common import htmlutil

FLAGS = flags.FLAGS
flags.DEFINE_boolean('debug_tokens', False,
                     'Whether to print all tokens for debugging.')

flags.DEFINE_boolean('error_trace', False,
                     'Whether to show error exceptions.')


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

  def Finalize(self, parser_state, tokenizer_mode):
    """Perform all checks that need to occur after all lines are processed.

    Args:
      parser_state: State of the parser after parsing all tokens
      tokenizer_mode: Mode of the tokenizer after parsing the entire page

    Raises:
      TypeError: If not overridden.
    """
    raise TypeError('Abstract method Finalize not implemented')


class CheckerBase(object):
  """This class handles checking a LintRules object against a file."""

  def __init__(self, error_handler, lint_rules, state_tracker,
               limited_doc_files=None, metadata_pass=None):
    """Initialize a checker object.

    Args:
      error_handler: Object that handles errors.
      lint_rules: LintRules object defining lint errors given a token
        and state_tracker object.
      state_tracker: Object that tracks the current state in the token stream.
      limited_doc_files: List of filenames that are not required to have
        documentation comments.
      metadata_pass: Object that builds metadata about the token stream.
    """
    self._error_handler = error_handler
    self._lint_rules = lint_rules
    self._state_tracker = state_tracker
    self._metadata_pass = metadata_pass
    self._limited_doc_files = limited_doc_files

    # TODO(user): Factor out. A checker does not need to know about the
    # tokenizer, only the token stream.
    self._tokenizer = javascripttokenizer.JavaScriptTokenizer()

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

  def Check(self, filename, source=None):
    """Checks the file, printing warnings and errors as they are found.

    Args:
      filename: The name of the file to check.
      source: Optional. The contents of the file.  Can be either a string or
          file-like object.  If omitted, contents will be read from disk from
          the given filename.
    """

    if source is None:
      try:
        f = open(filename)
      except IOError:
        self._error_handler.HandleFile(filename, None)
        self.HandleError(errors.FILE_NOT_FOUND, 'File not found', None)
        self._error_handler.FinishFile()
        return
    else:
      if type(source) in [str, unicode]:
        f = StringIO.StringIO(source)
      else:
        f = source

    try:
      if filename.endswith('.html') or filename.endswith('.htm'):
        self.CheckLines(filename, htmlutil.GetScriptLines(f), True)
      else:
        self.CheckLines(filename, f, False)
    finally:
      f.close()

  def CheckLines(self, filename, lines_iter, is_html):
    """Checks a file, given as an iterable of lines, for warnings and errors.

    Args:
      filename: The name of the file to check.
      lines_iter: An iterator that yields one line of the file at a time.
      is_html: Whether the file being checked is an HTML file with extracted
          contents.

    Returns:
      A boolean indicating whether the full file could be checked or if checking
      failed prematurely.
    """
    limited_doc_checks = False
    if self._limited_doc_files:
      for limited_doc_filename in self._limited_doc_files:
        if filename.endswith(limited_doc_filename):
          limited_doc_checks = True
          break

    lint_rules = self._lint_rules
    lint_rules.Initialize(self, limited_doc_checks, is_html)

    token = self._tokenizer.TokenizeFile(lines_iter)

    parse_error = None
    if self._metadata_pass:
      try:
        self._metadata_pass.Reset()
        self._metadata_pass.Process(token)
      except ecmametadatapass.ParseError, caught_parse_error:
        if FLAGS.error_trace:
          traceback.print_exc()
        parse_error = caught_parse_error
      except Exception:
        print 'Internal error in %s' % filename
        traceback.print_exc()
        return False

    self._error_handler.HandleFile(filename, token)

    return self._CheckTokens(token, parse_error=parse_error,
                             debug_tokens=FLAGS.debug_tokens)

  def _CheckTokens(self, token, parse_error, debug_tokens):
    """Checks a token stream for lint warnings/errors.

    Args:
      token: The first token in the token stream to check.
      parse_error: A ParseError if any errors occurred.
      debug_tokens: Whether every token should be printed as it is encountered
          during the pass.

    Returns:
      A boolean indicating whether the full token stream could be checked or if
      checking failed prematurely.
    """
    result = self._ExecutePass(token, self._LintPass, parse_error, debug_tokens)

    if not result:
      return False

    self._lint_rules.Finalize(self._state_tracker, self._tokenizer.mode)
    self._error_handler.FinishFile()
    return True

  def _LintPass(self, token):
    """Checks an individual token for lint warnings/errors.

    Used to encapsulate the logic needed to check an individual token so that it
    can be passed to _ExecutePass.

    Args:
      token: The token to check.
    """
    self._lint_rules.CheckToken(token, self._state_tracker)

  def _ExecutePass(self, token, pass_function, parse_error=None,
                   debug_tokens=False):
    """Calls the given function for every token in the given token stream.

    As each token is passed to the given function, state is kept up to date and,
    depending on the error_trace flag, errors are either caught and reported, or
    allowed to bubble up so developers can see the full stack trace. If a parse
    error is specified, the pass will proceed as normal until the token causing
    the parse error is reached.

    Args:
      token: The first token in the token stream.
      pass_function: The function to call for each token in the token stream.
      parse_error: A ParseError if any errors occurred.
      debug_tokens: Whether every token should be printed as it is encountered
          during the pass.

    Returns:
      A boolean indicating whether the full token stream could be checked or if
      checking failed prematurely.

    Raises:
      Exception: If any error occurred while calling the given function.
    """
    self._state_tracker.Reset()
    while token:
      if debug_tokens:
        print token

      if parse_error and parse_error.token == token:
        message = ('Error parsing file at token "%s". Unable to '
                   'check the rest of file.' % token.string)
        self.HandleError(errors.FILE_DOES_NOT_PARSE, message, token)
        self._error_handler.FinishFile()
        return

      try:
        self._state_tracker.HandleToken(
            token, self._state_tracker.GetLastNonSpaceToken())
        pass_function(token)
        self._state_tracker.HandleAfterToken(token)
      except:
        if FLAGS.error_trace:
          raise
        else:
          self.HandleError(errors.FILE_DOES_NOT_PARSE,
                           ('Error parsing file at token "%s". Unable to '
                            'check the rest of file.' % token.string),
                           token)
          self._error_handler.FinishFile()
        return False
      token = token.next
    return True
