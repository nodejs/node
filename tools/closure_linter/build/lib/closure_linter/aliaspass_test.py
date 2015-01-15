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

"""Unit tests for the aliaspass module."""

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

__author__ = ('nnaze@google.com (Nathan Naze)')

import unittest as googletest

from closure_linter import aliaspass
from closure_linter import errors
from closure_linter import javascriptstatetracker
from closure_linter import testutil
from closure_linter.common import erroraccumulator


def _GetTokenByLineAndString(start_token, string, line_number):
  for token in start_token:
    if token.line_number == line_number and token.string == string:
      return token


class AliasPassTest(googletest.TestCase):

  def testInvalidGoogScopeCall(self):
    start_token = testutil.TokenizeSourceAndRunEcmaPass(_TEST_SCOPE_SCRIPT)

    error_accumulator = erroraccumulator.ErrorAccumulator()
    alias_pass = aliaspass.AliasPass(
        error_handler=error_accumulator)
    alias_pass.Process(start_token)

    alias_errors = error_accumulator.GetErrors()
    self.assertEquals(1, len(alias_errors))

    alias_error = alias_errors[0]

    self.assertEquals(errors.INVALID_USE_OF_GOOG_SCOPE, alias_error.code)
    self.assertEquals('goog.scope', alias_error.token.string)

  def testAliasedIdentifiers(self):
    start_token = testutil.TokenizeSourceAndRunEcmaPass(_TEST_ALIAS_SCRIPT)
    alias_pass = aliaspass.AliasPass(set(['goog', 'myproject']))
    alias_pass.Process(start_token)

    alias_token = _GetTokenByLineAndString(start_token, 'Event', 4)
    self.assertTrue(alias_token.metadata.is_alias_definition)

    my_class_token = _GetTokenByLineAndString(start_token, 'myClass', 9)
    self.assertIsNone(my_class_token.metadata.aliased_symbol)

    component_token = _GetTokenByLineAndString(start_token, 'Component', 17)
    self.assertEquals('goog.ui.Component',
                      component_token.metadata.aliased_symbol)

    event_token = _GetTokenByLineAndString(start_token, 'Event.Something', 17)
    self.assertEquals('goog.events.Event.Something',
                      event_token.metadata.aliased_symbol)

    non_closurized_token = _GetTokenByLineAndString(
        start_token, 'NonClosurizedClass', 18)
    self.assertIsNone(non_closurized_token.metadata.aliased_symbol)

    long_start_token = _GetTokenByLineAndString(start_token, 'Event', 24)
    self.assertEquals('goog.events.Event.MultilineIdentifier.someMethod',
                      long_start_token.metadata.aliased_symbol)

  def testAliasedDoctypes(self):
    """Tests that aliases are correctly expanded within type annotations."""
    start_token = testutil.TokenizeSourceAndRunEcmaPass(_TEST_ALIAS_SCRIPT)
    tracker = javascriptstatetracker.JavaScriptStateTracker()
    tracker.DocFlagPass(start_token, error_handler=None)

    alias_pass = aliaspass.AliasPass(set(['goog', 'myproject']))
    alias_pass.Process(start_token)

    flag_token = _GetTokenByLineAndString(start_token, '@type', 22)
    self.assertEquals(
        'goog.events.Event.<goog.ui.Component,Array<myproject.foo.MyClass>>',
        repr(flag_token.attached_object.jstype))

  def testModuleAlias(self):
    start_token = testutil.TokenizeSourceAndRunEcmaPass("""
goog.module('goog.test');
var Alias = goog.require('goog.Alias');
Alias.use();
""")
    alias_pass = aliaspass.AliasPass(set(['goog']))
    alias_pass.Process(start_token)
    alias_token = _GetTokenByLineAndString(start_token, 'Alias', 3)
    self.assertTrue(alias_token.metadata.is_alias_definition)

  def testMultipleGoogScopeCalls(self):
    start_token = testutil.TokenizeSourceAndRunEcmaPass(
        _TEST_MULTIPLE_SCOPE_SCRIPT)

    error_accumulator = erroraccumulator.ErrorAccumulator()

    alias_pass = aliaspass.AliasPass(
        set(['goog', 'myproject']),
        error_handler=error_accumulator)
    alias_pass.Process(start_token)

    alias_errors = error_accumulator.GetErrors()

    self.assertEquals(3, len(alias_errors))

    error = alias_errors[0]
    self.assertEquals(errors.INVALID_USE_OF_GOOG_SCOPE, error.code)
    self.assertEquals(7, error.token.line_number)

    error = alias_errors[1]
    self.assertEquals(errors.EXTRA_GOOG_SCOPE_USAGE, error.code)
    self.assertEquals(7, error.token.line_number)

    error = alias_errors[2]
    self.assertEquals(errors.EXTRA_GOOG_SCOPE_USAGE, error.code)
    self.assertEquals(11, error.token.line_number)


_TEST_ALIAS_SCRIPT = """
goog.scope(function() {
var events = goog.events; // scope alias
var Event = events.
    Event; // nested multiline scope alias

// This should not be registered as an aliased identifier because
// it appears before the alias.
var myClass = new MyClass();

var Component = goog.ui.Component; // scope alias
var MyClass = myproject.foo.MyClass; // scope alias

// Scope alias of non-Closurized namespace.
var NonClosurizedClass = aaa.bbb.NonClosurizedClass;

var component = new Component(Event.Something);
var nonClosurized = NonClosurizedClass();

/**
 * A created namespace with a really long identifier.
 * @type {events.Event.<Component,Array<MyClass>}
 */
Event.
    MultilineIdentifier.
        someMethod = function() {};
});
"""

_TEST_SCOPE_SCRIPT = """
function foo () {
  // This goog.scope call is invalid.
  goog.scope(function() {

  });
}
"""

_TEST_MULTIPLE_SCOPE_SCRIPT = """
goog.scope(function() {
  // do nothing
});

function foo() {
  var test = goog.scope; // We should not see goog.scope mentioned.
}

// This goog.scope invalid. There can be only one.
goog.scope(function() {

});
"""


if __name__ == '__main__':
  googletest.main()
