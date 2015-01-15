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

from closure_linter import aliaspass
from closure_linter import checkerbase
from closure_linter import closurizednamespacesinfo
from closure_linter import javascriptlintrules


flags.DEFINE_list('closurized_namespaces', '',
                  'Namespace prefixes, used for testing of'
                  'goog.provide/require')
flags.DEFINE_list('ignored_extra_namespaces', '',
                  'Fully qualified namespaces that should be not be reported '
                  'as extra by the linter.')


class JavaScriptStyleChecker(checkerbase.CheckerBase):
  """Checker that applies JavaScriptLintRules."""

  def __init__(self, state_tracker, error_handler):
    """Initialize an JavaScriptStyleChecker object.

    Args:
      state_tracker: State tracker.
      error_handler: Error handler to pass all errors to.
    """
    self._namespaces_info = None
    self._alias_pass = None
    if flags.FLAGS.closurized_namespaces:
      self._namespaces_info = (
          closurizednamespacesinfo.ClosurizedNamespacesInfo(
              flags.FLAGS.closurized_namespaces,
              flags.FLAGS.ignored_extra_namespaces))

      self._alias_pass = aliaspass.AliasPass(
          flags.FLAGS.closurized_namespaces, error_handler)

    checkerbase.CheckerBase.__init__(
        self,
        error_handler=error_handler,
        lint_rules=javascriptlintrules.JavaScriptLintRules(
            self._namespaces_info),
        state_tracker=state_tracker)

  def Check(self, start_token, limited_doc_checks=False, is_html=False,
            stop_token=None):
    """Checks a token stream for lint warnings/errors.

    Adds a separate pass for computing dependency information based on
    goog.require and goog.provide statements prior to the main linting pass.

    Args:
      start_token: The first token in the token stream.
      limited_doc_checks: Whether to perform limited checks.
      is_html: Whether this token stream is HTML.
      stop_token: If given, checks should stop at this token.
    """
    self._lint_rules.Initialize(self, limited_doc_checks, is_html)

    self._state_tracker.DocFlagPass(start_token, self._error_handler)

    if self._alias_pass:
      self._alias_pass.Process(start_token)

    # To maximize the amount of errors that get reported before a parse error
    # is displayed, don't run the dependency pass if a parse error exists.
    if self._namespaces_info:
      self._namespaces_info.Reset()
      self._ExecutePass(start_token, self._DependencyPass, stop_token)

    self._ExecutePass(start_token, self._LintPass, stop_token)

    # If we have a stop_token, we didn't end up reading the whole file and,
    # thus, don't call Finalize to do end-of-file checks.
    if not stop_token:
      self._lint_rules.Finalize(self._state_tracker)

  def _DependencyPass(self, token):
    """Processes an individual token for dependency information.

    Used to encapsulate the logic needed to process an individual token so that
    it can be passed to _ExecutePass.

    Args:
      token: The token to process.
    """
    self._namespaces_info.ProcessToken(token, self._state_tracker)
