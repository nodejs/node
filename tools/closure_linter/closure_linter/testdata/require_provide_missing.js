// Copyright 2008 The Closure Linter Authors. All Rights Reserved.
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
 * @fileoverview The same code as require_provide_ok, but missing a provide
 * and a require call.
 *
 */

goog.provide('goog.something'); // +1: MISSING_GOOG_PROVIDE
// Missing provide of goog.something.Else and goog.something.SomeTypeDef.

goog.require('goog.Class');
goog.require('goog.package'); // +1: MISSING_GOOG_REQUIRE
// Missing requires of goog.Class.Enum and goog.otherThing.Class.Enum.


var x = new goog.Class();
goog.package.staticFunction();

var y = goog.Class.Enum.VALUE;


/**
 * @typedef {string}
 */
goog.something.SomeTypeDef;


/**
 * Private variable.
 * @type {number}
 * @private
 */
goog.something.private_ = 10;


/**
 * Use private variables defined in this file so they don't cause a warning.
 */
goog.something.usePrivateVariables = function() {
  var x = [
    goog.something.private_,
    x
  ];
};


/**
 * Static function.
 */
goog.something.staticFunction = function() {
};



/**
 * Constructor for Else.
 * @constructor
 */
goog.something.Else = function() {
  // Bug 1801608: Provide goog.otherThing.Class.Enum isn't missing.
  var enum = goog.otherThing.Class.Enum;
  goog.otherThing.Class.Enum = enum;
};
