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
 * @fileoverview There is nothing wrong w/ this javascript.
 *
 */
goog.module('goog.super.long.DependencyNameThatForcesMethodDefinitionToSpanMultipleLinesFooBar');
goog.provide('goog.something');
goog.provide('goog.something.Else');
goog.provide('goog.something.Else.Enum');
/** @suppress {extraProvide} */
goog.provide('goog.something.Extra');
goog.provide('goog.something.SomeTypeDef');
goog.provide('goog.somethingelse.someMethod');
goog.provide('goog.super.long.DependencyNameThatForcesTheLineToBeOverEightyCharacters');
goog.provide('notInClosurizedNamespacesSoNotExtra');

goog.require('dummy.foo');
goog.require('dummy.foo.someSpecificallyRequiredMethod');
goog.require('goog.Class');
/** @suppress {extraRequire} */
goog.require('goog.extra.require');
goog.require('goog.package');
goog.require('goog.package.ClassName');
goog.require('goog.package.OtherClassName');
/** @suppress {extraRequire} Legacy dependency on enum */
goog.require('goog.package.OuterClassName.InnerClassName');
goog.require('goog.super.long.DependencyNameThatForcesMethodDefinitionToSpanMultipleLinesFooBar');
goog.require('goog.super.long.DependencyNameThatForcesTheLineToBeOverEightyCharacters2');
goog.require('goog.super.long.DependencyNameThatForcesTheLineToBeOverEightyCharacters3');
goog.require('notInClosurizedNamespacesSoNotExtra');

dummy.foo.someMethod();
dummy.foo.someSpecificallyRequiredMethod();


// Regression test for bug 3473189. Both of these 'goog.provide' tokens should
// be completely ignored by alphabetization checks.
if (typeof goog != 'undefined' && typeof goog.provide == 'function') {
  goog.provide('goog.something.SomethingElse');
}


var x = new goog.Class();
goog.package.staticFunction();

var y = goog.Class.Enum.VALUE;


// This should not trigger a goog.require.
var somethingPrivate = goog.somethingPrivate.PrivateEnum_.VALUE;


/**
 * This method is provided directly, instead of its namespace.
 */
goog.somethingelse.someMethod = function() {};


/**
 * Defining a private property on a required namespace should not trigger a
 * provide of that namespace. Yes, people actually do this.
 * @private
 */
goog.Class.privateProperty_ = 1;


/**
 * @typedef {string}
 */
goog.something.SomeTypeDef;


/**
 * @typedef {string}
 * @private
 */
goog.something.SomePrivateTypeDef_;


/**
 * Some variable that is declared but not initialized.
 * @type {string|undefined}
 * @private
 */
goog.something.somePrivateVariable_;


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
    goog.Class.privateProperty_,
    x
  ];
};



/**
 * A really long class name to provide and usage of a really long class name to
 * be required.
 * @constructor
 */
goog.super.long.DependencyNameThatForcesTheLineToBeOverEightyCharacters =
    function() {
  var x = new goog.super.long. // LINE_ENDS_WITH_DOT
      DependencyNameThatForcesTheLineToBeOverEightyCharacters2();
  var x = new goog.super.long
      .DependencyNameThatForcesTheLineToBeOverEightyCharacters3();
  // Use x to avoid a warning.
  var x = [x];
};


/**
 * A really long class name to to force a method definition to be greater than
 * 80 lines. We should be grabbing the whole identifier regardless of how many
 * lines it is on.
 */
goog.super.long
    .DependencyNameThatForcesMethodDefinitionToSpanMultipleLinesFooBar
        .prototype.someMethod = function() {
};


/**
 * Static function.
 */
goog.something.staticFunction = function() {
  // Tests that namespace usages are identified using 'namespace.' not just
  // 'namespace'.
  googSomething.property;
  dummySomething.property;
  goog.package.ClassName        // A comment in between the identifier pieces.
      .IDENTIFIER_SPLIT_OVER_MULTIPLE_LINES;
  goog.package.OtherClassName.property = 1;

  // Test case where inner class needs to be required explicitly.
  new goog.package.OuterClassName.InnerClassName();

  // Don't just use goog.bar for missing namespace, hard coded to never require
  // goog since it's never provided.
  control.createConstructorMock(
      /** @suppress {missingRequire} */ goog.foo.bar, 'Baz');

  goog.require('goog.shouldBeIgnored');
};



/**
 * Constructor for Else.
 * @constructor
 */
goog.something.Else = function() {
  /** @suppress {missingRequire} */
  this.control.createConstructorMock(goog.foo.bar, 'Baz');
};


/**
 * Enum attached to Else.  Should not need to be provided explicitly, but
 * should not generate an extra require warning either.
 * @enum {number}
 */
goog.something.Else.Enum = {
  'key': 1
};


/**
 * Sample of a typedef.  This should not need a provide as it is an inner
 * element like an enum.
 *
 * @typedef {{val1: string, val2: boolean, val3: number}}
 */
goog.something.Else.Typedef;



/**
 * Constructor for SomethingElse.
 * @constructor
 */
goog.something.SomethingElse = function() {};


/**
 * @suppress {missingProvide}
 */
goog.suppress.someMethod = function() {};
