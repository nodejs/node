#!/usr/bin/env python
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
from closure_linter import error_fixer
from closure_linter import runner


_RESOURCE_PREFIX = 'closure_linter/testdata'

flags.FLAGS.strict = True
flags.FLAGS.limited_doc_files = ('dummy.js', 'externs.js')
flags.FLAGS.closurized_namespaces = ('goog', 'dummy')


class FixJsStyleTest(googletest.TestCase):
  """Test case to for gjslint auto-fixing."""

  def setUp(self):
    flags.FLAGS.dot_on_next_line = True

  def tearDown(self):
    flags.FLAGS.dot_on_next_line = False

  def testFixJsStyle(self):
    test_cases = [
        ['fixjsstyle.in.js', 'fixjsstyle.out.js'],
        ['indentation.js', 'fixjsstyle.indentation.out.js'],
        ['fixjsstyle.html.in.html', 'fixjsstyle.html.out.html'],
        ['fixjsstyle.oplineend.in.js', 'fixjsstyle.oplineend.out.js']]
    for [running_input_file, running_output_file] in test_cases:
      print 'Checking %s vs %s' % (running_input_file, running_output_file)
      input_filename = None
      golden_filename = None
      current_filename = None
      try:
        input_filename = '%s/%s' % (_RESOURCE_PREFIX, running_input_file)
        current_filename = input_filename

        golden_filename = '%s/%s' % (_RESOURCE_PREFIX, running_output_file)
        current_filename = golden_filename
      except IOError as ex:
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
      runner.Run(input_filename, error_fixer.ErrorFixer(actual))

      # Now compare the files.
      actual.seek(0)
      expected = open(golden_filename, 'r')

      # Uncomment to generate new golden files and run
      # open('/'.join(golden_filename.split('/')[4:]), 'w').write(actual.read())
      # actual.seek(0)

      self.assertEqual(actual.readlines(), expected.readlines())

  def testAddProvideFirstLine(self):
    """Tests handling of case where goog.provide is added."""
    original = [
        'dummy.bb.cc = 1;',
        ]

    expected = [
        'goog.provide(\'dummy.bb\');',
        '',
        'dummy.bb.cc = 1;',
        ]

    self._AssertFixes(original, expected, include_header=False)

    original = [
        '',
        'dummy.bb.cc = 1;',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testAddRequireFirstLine(self):
    """Tests handling of case where goog.require is added."""
    original = [
        'a = dummy.bb.cc;',
        ]

    expected = [
        'goog.require(\'dummy.bb\');',
        '',
        'a = dummy.bb.cc;',
        ]

    self._AssertFixes(original, expected, include_header=False)

    original = [
        '',
        'a = dummy.bb.cc;',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testDeleteProvideAndAddProvideFirstLine(self):
    """Tests handling of case where goog.provide is deleted and added.

       Bug 14832597.
    """
    original = [
        'goog.provide(\'dummy.aa\');',
        '',
        'dummy.bb.cc = 1;',
        ]

    expected = [
        'goog.provide(\'dummy.bb\');',
        '',
        'dummy.bb.cc = 1;',
        ]

    self._AssertFixes(original, expected, include_header=False)

    original = [
        'goog.provide(\'dummy.aa\');',
        'dummy.bb.cc = 1;',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testDeleteProvideAndAddRequireFirstLine(self):
    """Tests handling where goog.provide is deleted and goog.require added.

       Bug 14832597.
    """
    original = [
        'goog.provide(\'dummy.aa\');',
        '',
        'a = dummy.bb.cc;',
        ]

    expected = [
        'goog.require(\'dummy.bb\');',
        '',
        'a = dummy.bb.cc;',
        ]

    self._AssertFixes(original, expected, include_header=False)

    original = [
        'goog.provide(\'dummy.aa\');',
        'a = dummy.bb.cc;',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testDeleteRequireAndAddRequireFirstLine(self):
    """Tests handling of case where goog.require is deleted and added.

       Bug 14832597.
    """
    original = [
        'goog.require(\'dummy.aa\');',
        '',
        'a = dummy.bb.cc;',
        ]

    expected = [
        'goog.require(\'dummy.bb\');',
        '',
        'a = dummy.bb.cc;',
        ]

    self._AssertFixes(original, expected, include_header=False)

    original = [
        'goog.require(\'dummy.aa\');',
        'a = dummy.bb.cc;',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testDeleteRequireAndAddProvideFirstLine(self):
    """Tests handling where goog.require is deleted and goog.provide added.

       Bug 14832597.
    """
    original = [
        'goog.require(\'dummy.aa\');',
        '',
        'dummy.bb.cc = 1;',
        ]

    expected = [
        'goog.provide(\'dummy.bb\');',
        '',
        'dummy.bb.cc = 1;',
        ]

    self._AssertFixes(original, expected, include_header=False)

    original = [
        'goog.require(\'dummy.aa\');',
        'dummy.bb.cc = 1;',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testMultipleProvideInsert(self):
    original = [
        'goog.provide(\'dummy.bb\');',
        'goog.provide(\'dummy.dd\');',
        '',
        'dummy.aa.ff = 1;',
        'dummy.bb.ff = 1;',
        'dummy.cc.ff = 1;',
        'dummy.dd.ff = 1;',
        'dummy.ee.ff = 1;',
        ]

    expected = [
        'goog.provide(\'dummy.aa\');',
        'goog.provide(\'dummy.bb\');',
        'goog.provide(\'dummy.cc\');',
        'goog.provide(\'dummy.dd\');',
        'goog.provide(\'dummy.ee\');',
        '',
        'dummy.aa.ff = 1;',
        'dummy.bb.ff = 1;',
        'dummy.cc.ff = 1;',
        'dummy.dd.ff = 1;',
        'dummy.ee.ff = 1;',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testMultipleRequireInsert(self):
    original = [
        'goog.require(\'dummy.bb\');',
        'goog.require(\'dummy.dd\');',
        '',
        'a = dummy.aa.ff;',
        'b = dummy.bb.ff;',
        'c = dummy.cc.ff;',
        'd = dummy.dd.ff;',
        'e = dummy.ee.ff;',
        ]

    expected = [
        'goog.require(\'dummy.aa\');',
        'goog.require(\'dummy.bb\');',
        'goog.require(\'dummy.cc\');',
        'goog.require(\'dummy.dd\');',
        'goog.require(\'dummy.ee\');',
        '',
        'a = dummy.aa.ff;',
        'b = dummy.bb.ff;',
        'c = dummy.cc.ff;',
        'd = dummy.dd.ff;',
        'e = dummy.ee.ff;',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testUnsortedRequires(self):
    """Tests handling of unsorted goog.require statements without header.

       Bug 8398202.
    """
    original = [
        'goog.require(\'dummy.aa\');',
        'goog.require(\'dummy.Cc\');',
        'goog.require(\'dummy.Dd\');',
        '',
        'function a() {',
        '  dummy.aa.i = 1;',
        '  dummy.Cc.i = 1;',
        '  dummy.Dd.i = 1;',
        '}',
        ]

    expected = [
        'goog.require(\'dummy.Cc\');',
        'goog.require(\'dummy.Dd\');',
        'goog.require(\'dummy.aa\');',
        '',
        'function a() {',
        '  dummy.aa.i = 1;',
        '  dummy.Cc.i = 1;',
        '  dummy.Dd.i = 1;',
        '}',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testMissingExtraAndUnsortedRequires(self):
    """Tests handling of missing extra and unsorted goog.require statements."""
    original = [
        'goog.require(\'dummy.aa\');',
        'goog.require(\'dummy.Cc\');',
        'goog.require(\'dummy.Dd\');',
        '',
        'var x = new dummy.Bb();',
        'dummy.Cc.someMethod();',
        'dummy.aa.someMethod();',
        ]

    expected = [
        'goog.require(\'dummy.Bb\');',
        'goog.require(\'dummy.Cc\');',
        'goog.require(\'dummy.aa\');',
        '',
        'var x = new dummy.Bb();',
        'dummy.Cc.someMethod();',
        'dummy.aa.someMethod();',
        ]

    self._AssertFixes(original, expected)

  def testExtraRequireOnFirstLine(self):
    """Tests handling of extra goog.require statement on the first line.

       There was a bug when fixjsstyle quits with an exception. It happened if
        - the first line of the file is an extra goog.require() statement,
        - goog.require() statements are not sorted.
    """
    original = [
        'goog.require(\'dummy.aa\');',
        'goog.require(\'dummy.cc\');',
        'goog.require(\'dummy.bb\');',
        '',
        'var x = new dummy.bb();',
        'var y = new dummy.cc();',
        ]

    expected = [
        'goog.require(\'dummy.bb\');',
        'goog.require(\'dummy.cc\');',
        '',
        'var x = new dummy.bb();',
        'var y = new dummy.cc();',
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testUnsortedProvides(self):
    """Tests handling of unsorted goog.provide statements without header.

       Bug 8398202.
    """
    original = [
        'goog.provide(\'dummy.aa\');',
        'goog.provide(\'dummy.Cc\');',
        'goog.provide(\'dummy.Dd\');',
        '',
        'dummy.aa = function() {};'
        'dummy.Cc = function() {};'
        'dummy.Dd = function() {};'
        ]

    expected = [
        'goog.provide(\'dummy.Cc\');',
        'goog.provide(\'dummy.Dd\');',
        'goog.provide(\'dummy.aa\');',
        '',
        'dummy.aa = function() {};'
        'dummy.Cc = function() {};'
        'dummy.Dd = function() {};'
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testMissingExtraAndUnsortedProvides(self):
    """Tests handling of missing extra and unsorted goog.provide statements."""
    original = [
        'goog.provide(\'dummy.aa\');',
        'goog.provide(\'dummy.Cc\');',
        'goog.provide(\'dummy.Dd\');',
        '',
        'dummy.Cc = function() {};',
        'dummy.Bb = function() {};',
        'dummy.aa.someMethod = function();',
        ]

    expected = [
        'goog.provide(\'dummy.Bb\');',
        'goog.provide(\'dummy.Cc\');',
        'goog.provide(\'dummy.aa\');',
        '',
        'dummy.Cc = function() {};',
        'dummy.Bb = function() {};',
        'dummy.aa.someMethod = function();',
        ]

    self._AssertFixes(original, expected)

  def testNoRequires(self):
    """Tests positioning of missing requires without existing requires."""
    original = [
        'goog.provide(\'dummy.Something\');',
        '',
        'dummy.Something = function() {};',
        '',
        'var x = new dummy.Bb();',
        ]

    expected = [
        'goog.provide(\'dummy.Something\');',
        '',
        'goog.require(\'dummy.Bb\');',
        '',
        'dummy.Something = function() {};',
        '',
        'var x = new dummy.Bb();',
        ]

    self._AssertFixes(original, expected)

  def testNoProvides(self):
    """Tests positioning of missing provides without existing provides."""
    original = [
        'goog.require(\'dummy.Bb\');',
        '',
        'dummy.Something = function() {};',
        '',
        'var x = new dummy.Bb();',
        ]

    expected = [
        'goog.provide(\'dummy.Something\');',
        '',
        'goog.require(\'dummy.Bb\');',
        '',
        'dummy.Something = function() {};',
        '',
        'var x = new dummy.Bb();',
        ]

    self._AssertFixes(original, expected)

  def testOutputOkayWhenFirstTokenIsDeleted(self):
    """Tests that autofix output is is correct when first token is deleted.

    Regression test for bug 4581567
    """
    original = ['"use strict";']
    expected = ["'use strict';"]

    self._AssertFixes(original, expected, include_header=False)

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

  def testEndsWithIdentifier(self):
    """Tests Handling case where script ends with identifier. Bug 7643404."""
    original = [
        'goog.provide(\'xyz\');',
        '',
        'abc'
        ]

    expected = [
        'goog.provide(\'xyz\');',
        '',
        'abc;'
        ]

    self._AssertFixes(original, expected)

  def testFileStartsWithSemicolon(self):
    """Tests handling files starting with semicolon.

      b/10062516
    """
    original = [
        ';goog.provide(\'xyz\');',
        '',
        'abc;'
        ]

    expected = [
        'goog.provide(\'xyz\');',
        '',
        'abc;'
        ]

    self._AssertFixes(original, expected, include_header=False)

  def testCodeStartsWithSemicolon(self):
    """Tests handling code in starting with semicolon after comments.

      b/10062516
    """
    original = [
        ';goog.provide(\'xyz\');',
        '',
        'abc;'
        ]

    expected = [
        'goog.provide(\'xyz\');',
        '',
        'abc;'
        ]

    self._AssertFixes(original, expected)

  def _AssertFixes(self, original, expected, include_header=True):
    """Asserts that the error fixer corrects original to expected."""
    if include_header:
      original = self._GetHeader() + original
      expected = self._GetHeader() + expected

    actual = StringIO.StringIO()
    runner.Run('testing.js', error_fixer.ErrorFixer(actual), original)
    actual.seek(0)

    expected = [x + '\n' for x in expected]

    self.assertListEqual(actual.readlines(), expected)

  def _GetHeader(self):
    """Returns a fake header for a JavaScript file."""
    return [
        '// Copyright 2011 Google Inc. All Rights Reserved.',
        '',
        '/**',
        ' * @fileoverview Fake file overview.',
        ' * @author fake@google.com (Fake Person)',
        ' */',
        ''
        ]


if __name__ == '__main__':
  googletest.main()
