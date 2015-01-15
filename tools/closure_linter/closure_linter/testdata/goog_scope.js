// Copyright 2011 The Closure Linter Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Tests provides/requires in the presence of goog.scope.
 * There should be no errors for missing provides or requires.
 *
 * @author nicksantos@google.com (Nick Santos)
 */

goog.provide('goog.something.Something');

goog.require('goog.util.Else');

goog.scope(function() {
var Else = goog.util.Else;
var something = goog.something;

/** // WRONG_BLANK_LINE_COUNT
 * This is a something.
 * @constructor
 */
something.Something = function() {
  /**
   * This is an else.
   * @type {Else}
   */
  this.myElse = new Else();

  /** @type {boolean} */
  this.private_ = false;  // MISSING_PRIVATE, UNUSED_PRIVATE_MEMBER
};

/** // WRONG_BLANK_LINE_COUNT
 * // +3: MISSING_PRIVATE
 * Missing private.
 */
something.withTrailingUnderscore_ = 'should be declared @private';

/** // WRONG_BLANK_LINE_COUNT
 * Does nothing.
 */
something.Something.prototype.noOp = function() {};


/**
 * Does something.
 * Tests for included semicolon in function expression in goog.scope.
 */
something.Something.prototype.someOp = function() {
} // MISSING_SEMICOLON_AFTER_FUNCTION
});  // goog.scope
