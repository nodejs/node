#!/usr/bin/env python
#
# Copyright 2012 The Closure Linter Authors. All Rights Reserved.
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

"""Main lint function. Tokenizes file, runs passes, and feeds to checker."""

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

__author__ = 'nnaze@google.com (Nathan Naze)'

import traceback

import gflags as flags

from closure_linter import checker
from closure_linter import ecmalintrules
from closure_linter import ecmametadatapass
from closure_linter import error_check
from closure_linter import errors
from closure_linter import javascriptstatetracker
from closure_linter import javascripttokenizer

from closure_linter.common import error
from closure_linter.common import htmlutil
from closure_linter.common import tokens

flags.DEFINE_list('limited_doc_files', ['dummy.js', 'externs.js'],
                  'List of files with relaxed documentation checks. Will not '
                  'report errors for missing documentation, some missing '
                  'descriptions, or methods whose @return tags don\'t have a '
                  'matching return statement.')
flags.DEFINE_boolean('error_trace', False,
                     'Whether to show error exceptions.')
flags.ADOPT_module_key_flags(checker)
flags.ADOPT_module_key_flags(ecmalintrules)
flags.ADOPT_module_key_flags(error_check)


def _GetLastNonWhiteSpaceToken(start_token):
  """Get the last non-whitespace token in a token stream."""
  ret_token = None

  whitespace_tokens = frozenset([
      tokens.TokenType.WHITESPACE, tokens.TokenType.BLANK_LINE])
  for t in start_token:
    if t.type not in whitespace_tokens:
      ret_token = t

  return ret_token


def _IsHtml(filename):
  return filename.endswith('.html') or filename.endswith('.htm')


def _Tokenize(fileobj):
  """Tokenize a file.

  Args:
    fileobj: file-like object (or iterable lines) with the source.

  Returns:
    The first token in the token stream and the ending mode of the tokenizer.
  """
  tokenizer = javascripttokenizer.JavaScriptTokenizer()
  start_token = tokenizer.TokenizeFile(fileobj)
  return start_token, tokenizer.mode


def _IsLimitedDocCheck(filename, limited_doc_files):
  """Whether this this a limited-doc file.

  Args:
    filename: The filename.
    limited_doc_files: Iterable of strings. Suffixes of filenames that should
      be limited doc check.

  Returns:
    Whether the file should be limited check.
  """
  for limited_doc_filename in limited_doc_files:
    if filename.endswith(limited_doc_filename):
      return True
  return False


def Run(filename, error_handler, source=None):
  """Tokenize, run passes, and check the given file.

  Args:
    filename: The path of the file to check
    error_handler: The error handler to report errors to.
    source: A file-like object with the file source. If omitted, the file will
      be read from the filename path.
  """
  if not source:
    try:
      source = open(filename)
    except IOError:
      error_handler.HandleFile(filename, None)
      error_handler.HandleError(
          error.Error(errors.FILE_NOT_FOUND, 'File not found'))
      error_handler.FinishFile()
      return

  if _IsHtml(filename):
    source_file = htmlutil.GetScriptLines(source)
  else:
    source_file = source

  token, tokenizer_mode = _Tokenize(source_file)

  error_handler.HandleFile(filename, token)

  # If we did not end in the basic mode, this a failed parse.
  if tokenizer_mode is not javascripttokenizer.JavaScriptModes.TEXT_MODE:
    error_handler.HandleError(
        error.Error(errors.FILE_IN_BLOCK,
                    'File ended in mode "%s".' % tokenizer_mode,
                    _GetLastNonWhiteSpaceToken(token)))

  # Run the ECMA pass
  error_token = None

  ecma_pass = ecmametadatapass.EcmaMetaDataPass()
  error_token = RunMetaDataPass(token, ecma_pass, error_handler, filename)

  is_limited_doc_check = (
      _IsLimitedDocCheck(filename, flags.FLAGS.limited_doc_files))

  _RunChecker(token, error_handler,
              is_limited_doc_check,
              is_html=_IsHtml(filename),
              stop_token=error_token)

  error_handler.FinishFile()


def RunMetaDataPass(start_token, metadata_pass, error_handler, filename=''):
  """Run a metadata pass over a token stream.

  Args:
    start_token: The first token in a token stream.
    metadata_pass: Metadata pass to run.
    error_handler: The error handler to report errors to.
    filename: Filename of the source.

  Returns:
    The token where the error occurred (if any).
  """

  try:
    metadata_pass.Process(start_token)
  except ecmametadatapass.ParseError, parse_err:
    if flags.FLAGS.error_trace:
      traceback.print_exc()
    error_token = parse_err.token
    error_msg = str(parse_err)
    error_handler.HandleError(
        error.Error(errors.FILE_DOES_NOT_PARSE,
                    ('Error parsing file at token "%s". Unable to '
                     'check the rest of file.'
                     '\nError "%s"' % (error_token, error_msg)), error_token))
    return error_token
  except Exception:  # pylint: disable=broad-except
    traceback.print_exc()
    error_handler.HandleError(
        error.Error(
            errors.FILE_DOES_NOT_PARSE,
            'Internal error in %s' % filename))


def _RunChecker(start_token, error_handler,
                limited_doc_checks, is_html,
                stop_token=None):

  state_tracker = javascriptstatetracker.JavaScriptStateTracker()

  style_checker = checker.JavaScriptStyleChecker(
      state_tracker=state_tracker,
      error_handler=error_handler)

  style_checker.Check(start_token,
                      is_html=is_html,
                      limited_doc_checks=limited_doc_checks,
                      stop_token=stop_token)
