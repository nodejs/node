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

"""Unit tests for the javascriptstatetracker module."""

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

__author__ = ('nnaze@google.com (Nathan Naze)')


import unittest as googletest

from closure_linter import javascripttokens
from closure_linter import testutil
from closure_linter import tokenutil


_FUNCTION_SCRIPT = """\
var a = 3;

function foo(aaa, bbb, ccc) {
  var b = 4;
}


/**
 * JSDoc comment.
 */
var bar = function(ddd, eee, fff) {

};


/**
 * Verify that nested functions get their proper parameters recorded.
 */
var baz = function(ggg, hhh, iii) {
  var qux = function(jjj, kkk, lll) {
  };
  // make sure that entering a new block does not change baz' parameters.
  {};
};

"""


class FunctionTest(googletest.TestCase):

  def testFunctionParse(self):
    functions, _ = testutil.ParseFunctionsAndComments(_FUNCTION_SCRIPT)
    self.assertEquals(4, len(functions))

    # First function
    function = functions[0]
    self.assertEquals(['aaa', 'bbb', 'ccc'], function.parameters)

    start_token = function.start_token
    end_token = function.end_token

    self.assertEquals(
        javascripttokens.JavaScriptTokenType.FUNCTION_DECLARATION,
        function.start_token.type)

    self.assertEquals('function', start_token.string)
    self.assertEquals(3, start_token.line_number)
    self.assertEquals(0, start_token.start_index)

    self.assertEquals('}', end_token.string)
    self.assertEquals(5, end_token.line_number)
    self.assertEquals(0, end_token.start_index)

    self.assertEquals('foo', function.name)

    self.assertIsNone(function.doc)

    # Second function
    function = functions[1]
    self.assertEquals(['ddd', 'eee', 'fff'], function.parameters)

    start_token = function.start_token
    end_token = function.end_token

    self.assertEquals(
        javascripttokens.JavaScriptTokenType.FUNCTION_DECLARATION,
        function.start_token.type)

    self.assertEquals('function', start_token.string)
    self.assertEquals(11, start_token.line_number)
    self.assertEquals(10, start_token.start_index)

    self.assertEquals('}', end_token.string)
    self.assertEquals(13, end_token.line_number)
    self.assertEquals(0, end_token.start_index)

    self.assertEquals('bar', function.name)

    self.assertIsNotNone(function.doc)

    # Check function JSDoc
    doc = function.doc
    doc_tokens = tokenutil.GetTokenRange(doc.start_token, doc.end_token)

    comment_type = javascripttokens.JavaScriptTokenType.COMMENT
    comment_tokens = filter(lambda t: t.type is comment_type, doc_tokens)

    self.assertEquals('JSDoc comment.',
                      tokenutil.TokensToString(comment_tokens).strip())

    # Third function
    function = functions[2]
    self.assertEquals(['ggg', 'hhh', 'iii'], function.parameters)

    start_token = function.start_token
    end_token = function.end_token

    self.assertEquals(
        javascripttokens.JavaScriptTokenType.FUNCTION_DECLARATION,
        function.start_token.type)

    self.assertEquals('function', start_token.string)
    self.assertEquals(19, start_token.line_number)
    self.assertEquals(10, start_token.start_index)

    self.assertEquals('}', end_token.string)
    self.assertEquals(24, end_token.line_number)
    self.assertEquals(0, end_token.start_index)

    self.assertEquals('baz', function.name)
    self.assertIsNotNone(function.doc)

    # Fourth function (inside third function)
    function = functions[3]
    self.assertEquals(['jjj', 'kkk', 'lll'], function.parameters)

    start_token = function.start_token
    end_token = function.end_token

    self.assertEquals(
        javascripttokens.JavaScriptTokenType.FUNCTION_DECLARATION,
        function.start_token.type)

    self.assertEquals('function', start_token.string)
    self.assertEquals(20, start_token.line_number)
    self.assertEquals(12, start_token.start_index)

    self.assertEquals('}', end_token.string)
    self.assertEquals(21, end_token.line_number)
    self.assertEquals(2, end_token.start_index)

    self.assertEquals('qux', function.name)
    self.assertIsNone(function.doc)



class CommentTest(googletest.TestCase):

  def testGetDescription(self):
    comment = self._ParseComment("""
        /**
         * Comment targeting goog.foo.
         *
         * This is the second line.
         * @param {number} foo The count of foo.
         */
        target;""")

    self.assertEqual(
        'Comment targeting goog.foo.\n\nThis is the second line.',
        comment.description)

  def testCommentGetTarget(self):
    self.assertCommentTarget('goog.foo', """
        /**
         * Comment targeting goog.foo.
         */
        goog.foo = 6;
        """)

    self.assertCommentTarget('bar', """
        /**
         * Comment targeting bar.
         */
        var bar = "Karate!";
        """)

    self.assertCommentTarget('doThing', """
        /**
         * Comment targeting doThing.
         */
        function doThing() {};
        """)

    self.assertCommentTarget('this.targetProperty', """
        goog.bar.Baz = function() {
          /**
           * Comment targeting targetProperty.
           */
          this.targetProperty = 3;
        };
        """)

    self.assertCommentTarget('goog.bar.prop', """
        /**
         * Comment targeting goog.bar.prop.
         */
        goog.bar.prop;
        """)

    self.assertCommentTarget('goog.aaa.bbb', """
        /**
         * Comment targeting goog.aaa.bbb.
         */
        (goog.aaa.bbb)
        """)

    self.assertCommentTarget('theTarget', """
        /**
         * Comment targeting symbol preceded by newlines, whitespace,
         * and parens -- things we ignore.
         */
        (theTarget)
        """)

    self.assertCommentTarget(None, """
        /**
         * @fileoverview File overview.
         */
        (notATarget)
        """)

    self.assertCommentTarget(None, """
        /**
         * Comment that doesn't find a target.
         */
        """)

    self.assertCommentTarget('theTarget.is.split.across.lines', """
        /**
         * Comment that addresses a symbol split across lines.
         */
        (theTarget.is.split
             .across.lines)
        """)

    self.assertCommentTarget('theTarget.is.split.across.lines', """
        /**
         * Comment that addresses a symbol split across lines.
         */
        (theTarget.is.split.
                across.lines)
        """)

  def _ParseComment(self, script):
    """Parse a script that contains one comment and return it."""
    _, comments = testutil.ParseFunctionsAndComments(script)
    self.assertEquals(1, len(comments))
    return comments[0]

  def assertCommentTarget(self, target, script):
    comment = self._ParseComment(script)
    self.assertEquals(target, comment.GetTargetIdentifier())


if __name__ == '__main__':
  googletest.main()
