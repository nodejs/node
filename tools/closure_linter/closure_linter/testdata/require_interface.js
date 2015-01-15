// Copyright 2010 The Closure Linter Authors. All Rights Reserved.
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
 * @fileoverview Contains a test to verify that interfaces implemented in a file
 * are goog.require'd.
 *
 */

// We're missing a goog.require of goog.something.SomeInterface.
goog.provide('goog.something.SomeClass'); // +1: MISSING_GOOG_REQUIRE



/**
 * Constructor for SomeClass.
 * @constructor
 * @implements {goog.something.SomeInterface}
 */
goog.something.SomeClass = function() {};

