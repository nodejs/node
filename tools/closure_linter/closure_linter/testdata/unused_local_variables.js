// Copyright 2013 The Closure Linter Authors. All Rights Reserved.
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
 * @fileoverview Checks that unused local variables result in an error.
 */

goog.provide('dummy.Something');



/**
 * @constructor
 */
dummy.Something = function() {
  // This variable isn't really used, but we can't tell for sure.
  var usedVariable = [];
  usedVariable.length = 1;

  var variableUsedInAClosure = [];
  var functionUsedByInvoking = function() {
    variableUsedInAClosure[1] = 'abc';
  };
  functionUsedByInvoking();

  var variableUsedTwoLevelsDeep = [];
  var firstLevelFunction = function() {
    function() {
      variableUsedTwoLevelsDeep.append(1);
    }
  };
  firstLevelFunction();

  // This variable isn't being declared so is unchecked.
  undeclaredLocal = 1;

  var unusedVariable;

  // Check that using a variable as member name doesn't trigger
  // usage.
  this.unusedVariable = 0;
  this.unusedVariable = this.unusedVariable + 1;

  // Check that declaring a variable twice doesn't trigger
  // usage.
  var unusedVariable; // UNUSED_LOCAL_VARIABLE

  var unusedVariableWithReassignment = [];  // UNUSED_LOCAL_VARIABLE
  unusedVariableWithReassignment = 'a';

  var unusedFunction = function() {}; // UNUSED_LOCAL_VARIABLE

  var unusedHiddenVariable = 1; // UNUSED_LOCAL_VARIABLE
  firstLevelFunction = function() {
    // This variable is actually used in the function below, but hides the outer
    // variable with the same name.
    var unusedHiddenVariable = 1;
    function() {
      delete unusedHiddenVariable;
    }
  };
};


goog.scope(function() {
var unusedAlias = dummy.Something;   // UNUSED_LOCAL_VARIABLE
var UsedTypeAlias = dummy.Something;
var AnotherUsedTypeAlias = dummy.Something;


/** @protected {AnotherUsedTypeAlias.Something|UsedTypeAlias} */
var usedAlias = dummy.Something;
new usedAlias();
});  // goog.scope

// Unused top level variables are not checked.
var unusedTopLevelVariable;
