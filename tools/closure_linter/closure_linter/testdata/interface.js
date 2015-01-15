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
 * @fileoverview Test file for interfaces.
 * @author robbyw@google.com (Robert Walker)
 */

goog.provide('sample.BadInterface');
goog.provide('sample.GoodInterface');



/**
 * Sample interface to demonstrate correct style.
 * @interface
 */
sample.GoodInterface = function() {
};


/**
 * Legal methods can take parameters and have a return type.
 * @param {string} param1 First parameter.
 * @param {Object} param2 Second parameter.
 * @return {number} Some return value.
 */
sample.GoodInterface.prototype.legalMethod = function(param1, param2) {
};


/**
 * Legal methods can also take no parameters and return nothing.
 */
sample.GoodInterface.prototype.legalMethod2 = function() {
  // Comments should be allowed.
};


/**
 * Legal methods can also be omitted, even with params and return values.
 * @param {string} param1 First parameter.
 * @param {Object} param2 Second parameter.
 * @return {number} Some return value.
 */
sample.GoodInterface.prototype.legalMethod3;


/**
 * Legal methods can also be set to abstract, even with params and return
 * values.
 * @param {string} param1 First parameter.
 * @param {Object} param2 Second parameter.
 * @return {number} Some return value.
 */
sample.GoodInterface.prototype.legalMethod4 = goog.abstractMethod;



/**
 * Sample interface to demonstrate style errors.
 * @param {string} a This is illegal.
 * @interface
 */
sample.BadInterface = function(a) { // INTERFACE_CONSTRUCTOR_CANNOT_HAVE_PARAMS
  this.x = a; // INTERFACE_METHOD_CANNOT_HAVE_CODE
};


/**
 * It is illegal to include code in an interface method.
 * @param {string} param1 First parameter.
 * @param {Object} param2 Second parameter.
 * @return {number} Some return value.
 */
sample.BadInterface.prototype.illegalMethod = function(param1, param2) {
  return 10; // INTERFACE_METHOD_CANNOT_HAVE_CODE
};
