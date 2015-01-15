#!/usr/bin/env python
#
# Copyright 2012 The Closure Linter Authors. All Rights Reserved.
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

"""Utility functions for testing gjslint components."""

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

__author__ = ('nnaze@google.com (Nathan Naze)')

import StringIO

from closure_linter import ecmametadatapass
from closure_linter import javascriptstatetracker
from closure_linter import javascripttokenizer


def TokenizeSource(source):
  """Convert a source into a string of tokens.

  Args:
    source: A source file as a string or file-like object (iterates lines).

  Returns:
    The first token of the resulting token stream.
  """

  if isinstance(source, basestring):
    source = StringIO.StringIO(source)

  tokenizer = javascripttokenizer.JavaScriptTokenizer()
  return tokenizer.TokenizeFile(source)


def TokenizeSourceAndRunEcmaPass(source):
  """Tokenize a source and run the EcmaMetaDataPass on it.

  Args:
    source: A source file as a string or file-like object (iterates lines).

  Returns:
    The first token of the resulting token stream.
  """
  start_token = TokenizeSource(source)
  ecma_pass = ecmametadatapass.EcmaMetaDataPass()
  ecma_pass.Process(start_token)
  return start_token


def ParseFunctionsAndComments(source, error_handler=None):
  """Run the tokenizer and tracker and return comments and functions found.

  Args:
    source: A source file as a string or file-like object (iterates lines).
    error_handler: An error handler.

  Returns:
    The functions and comments as a tuple.
  """
  start_token = TokenizeSourceAndRunEcmaPass(source)

  tracker = javascriptstatetracker.JavaScriptStateTracker()
  if error_handler is not None:
    tracker.DocFlagPass(start_token, error_handler)

  functions = []
  comments = []
  for token in start_token:
    tracker.HandleToken(token, tracker.GetLastNonSpaceToken())

    function = tracker.GetFunction()
    if function and function not in functions:
      functions.append(function)

    comment = tracker.GetDocComment()
    if comment and comment not in comments:
      comments.append(comment)

    tracker.HandleAfterToken(token)

  return functions, comments
