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
 * @fileoverview Tests missing requires around the usage of the require
 * suppression annotation.
 *
 */

// We are missing a require of goog.foo.
goog.provide('goog.something.Else'); // +1: MISSING_GOOG_REQUIRE



/**
 * Constructor for Else.
 * @constructor
 */
goog.something.Else = function() {
  /** @suppress {missingRequire} */
  this.control.createConstructorMock(
      goog.foo.bar, 'Baz');

  // Previous suppress should only be scoped to that statement.
  this.control.createConstructorMock(
      goog.foo.bar, 'Baz');

  this.control.invoke(goog.foo.bar, 'Test');
};
