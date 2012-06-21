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
    test_cases = [['fixjsstyle.in.js', 'fixjsstyle.out.js'],
                  ['indentation.js', 'fixjsstyle.indentation.out.js']]
    for [running_input_file, running_output_file] in test_cases:
      input_filename = None
      golden_filename = None
      current_filename = None
      try:
        input_filename = '%s/%s' % (_RESOURCE_PREFIX, running_input_file)
        current_filename = input_filename

        golden_filename = '%s/%s' % (_RESOURCE_PREFIX, running_output_file)
        current_filename = golden_filename
      except IOError, ex:
        raise IOError('Could not find testdata resource for %s: %s' %
                      (current_filename, ex))

      if running_input_file == 'fixjsstyle.in.js':
        with open(input_filename) as f:
          for line in f:
            # Go to last line.
            pass
          self.assertTrue(line == line.rstrip(), '%s file should not end '
                          'with a new line.' % (input_filename))

      # Autofix the file, sending output to a fake file.
      actual = StringIO.StringIO()
      style_checker = checker.JavaScriptStyleChecker(
          error_fixer.ErrorFixer(actual))
      style_checker.Check(input_filename)

      # Now compare the files.
      actual.seek(0)
      expected = open(golden_filename, 'r')

      self.assertEqual(actual.readlines(), expected.readlines())

  def testMissingExtraAndUnsortedRequires(self):
    """Tests handling of missing extra and unsorted goog.require statements."""
    original = [
        "goog.require('dummy.aa');",
        "goog.require('dummy.Cc');",
        "goog.require('dummy.Dd');",
        "",
        "var x = new dummy.Bb();",
        "dummy.Cc.someMethod();",
        "dummy.aa.someMethod();",
        ]

    expected = [
        "goog.require('dummy.Bb');",
        "goog.require('dummy.Cc');",
        "goog.require('dummy.aa');",
        "",
        "var x = new dummy.Bb();",
        "dummy.Cc.someMethod();",
        "dummy.aa.someMethod();",
        ]

    self._AssertFixes(original, expected)

  def testMissingExtraAndUnsortedProvides(self):
    """Tests handling of missing extra and unsorted goog.provide statements."""
    original = [
        "goog.provide('dummy.aa');",
        "goog.provide('dummy.Cc');",
        "goog.provide('dummy.Dd');",
        "",
        "dummy.Cc = function() {};",
        "dummy.Bb = function() {};",
        "dummy.aa.someMethod = function();",
        ]

    expected = [
        "goog.provide('dummy.Bb');",
        "goog.provide('dummy.Cc');",
        "goog.provide('dummy.aa');",
        "",
        "dummy.Cc = function() {};",
        "dummy.Bb = function() {};",
        "dummy.aa.someMethod = function();",
        ]

    self._AssertFixes(original, expected)

  def testNoRequires(self):
    """Tests positioning of missing requires without existing requires."""
    original = [
        "goog.provide('dummy.Something');",
        "",
        "dummy.Something = function() {};",
        "",
        "var x = new dummy.Bb();",
        ]

    expected = [
        "goog.provide('dummy.Something');",
        "",
        "goog.require('dummy.Bb');",
        "",
        "dummy.Something = function() {};",
        "",
        "var x = new dummy.Bb();",
        ]

    self._AssertFixes(original, expected)

  def testNoProvides(self):
    """Tests positioning of missing provides without existing provides."""
    original = [
        "goog.require('dummy.Bb');",
        "",
        "dummy.Something = function() {};",
        "",
        "var x = new dummy.Bb();",
        ]

    expected = [
        "goog.provide('dummy.Something');",
        "",
        "goog.require('dummy.Bb');",
        "",
        "dummy.Something = function() {};",
        "",
        "var x = new dummy.Bb();",
        ]

    self._AssertFixes(original, expected)

  def testGoogScopeIndentation(self):
    """Tests Handling a typical end-of-scope indentation fix."""
    original = [
        'goog.scope(function() {',
        '  // TODO(brain): Take over the world.',
        '});  // goog.scope',
        ]

    expected = [
        'goog.scope(function() {',
        '// TODO(brain): Take over the world.',
        '});  // goog.scope',
        ]

    self._AssertFixes(original, expected)

  def testMissingEndOfScopeComment(self):
    """Tests Handling a missing comment at end of goog.scope."""
    original = [
        'goog.scope(function() {',
        '});',
        ]

    expected = [
        'goog.scope(function() {',
        '});  // goog.scope',
        ]

    self._AssertFixes(original, expected)

  def testMissingEndOfScopeCommentWithOtherComment(self):
    """Tests handling an irrelevant comment at end of goog.scope."""
    original = [
        'goog.scope(function() {',
        "});  // I don't belong here!",
        ]

    expected = [
        'goog.scope(function() {',
        '});  // goog.scope',
        ]

    self._AssertFixes(original, expected)

  def testMalformedEndOfScopeComment(self):
    """Tests Handling a malformed comment at end of goog.scope."""
    original = [
        'goog.scope(function() {',
        '});  // goog.scope FTW',
        ]

    expected = [
        'goog.scope(function() {',
        '});  // goog.scope',
        ]

    self._AssertFixes(original, expected)

  def _AssertFixes(self, original, expected):
    """Asserts that the error fixer corrects original to expected."""
    original = self._GetHeader() + original
    expected = self._GetHeader() + expected

    actual = StringIO.StringIO()
    style_checker = checker.JavaScriptStyleChecker(
        error_fixer.ErrorFixer(actual))
    style_checker.CheckLines('testing.js', original, False)
    actual.seek(0)

    expected = [x + '\n' for x in expected]

    self.assertListEqual(actual.readlines(), expected)

  def _GetHeader(self):
    """Returns a fake header for a JavaScript file."""
    return [
        "// Copyright 2011 Google Inc. All Rights Reserved.",
        "",
        "/**",
        " * @fileoverview Fake file overview.",
        " * @author fake@google.com (Fake Person)",
        " */",
        ""
        ]


if __name__ == '__main__':
  googletest.main()
