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
 * @fileoverview Checks that an unused private members result in an error.
 */

goog.provide('dummy.Something');



/**
 * @constructor
 */
dummy.Something = function() {
  /**
   * @type {number}
   * @private
   */
  this.normalVariable_ = 1;

  // +5: UNUSED_PRIVATE_MEMBER
  /**
   * @type {number}
   * @private
   */
  this.unusedVariable_ = 1;

  /**
   * @type {number}
   * @private
   * @suppress {unusedPrivateMembers}
   */
  this.suppressedUnusedVariable_ = 1;
};


/**
 * @type {number}
 * @private
 */
dummy.Something.NORMAL_CONSTANT_ = 1;


// +5: UNUSED_PRIVATE_MEMBER
/**
 * @type {number}
 * @private
 */
dummy.Something.UNUSED_CONSTANT_ = 1;


/**
 * @type {number}
 * @private
 * @suppress {unusedPrivateMembers}
 */
dummy.Something.SUPPRESSED_UNUSED_CONSTANT_ = 1;


/**
 * @type {number}
 * @private
 */
dummy.Something.normalStaticVariable_ = 1;


// +5: UNUSED_PRIVATE_MEMBER
/**
 * @type {number}
 * @private
 */
dummy.Something.unusedStaticVariable_ = 1;


/**
 * @type {number}
 * @private
 * @suppress {unusedPrivateMembers}
 */
dummy.Something.suppressedUnusedStaticVariable_ = 1;


/**
 * @type {number}
 * @private
 */
dummy.Something.prototype.normalVariableOnPrototype_ = 1;


// +5: UNUSED_PRIVATE_MEMBER
/**
 * @type {number}
 * @private
 */
dummy.Something.prototype.unusedVariableOnPrototype_ = 1;


/**
 * @type {number}
 * @private
 * @suppress {unusedPrivateMembers}
 */
dummy.Something.prototype.suppressedUnusedVariableOnPrototype_ = 1;


/**
 * Check edge cases that should not be reported.
 */
dummy.Something.prototype.checkFalsePositives = function() {
  this.__iterator__ = 1;
  this.normalVariable_.unknownChainedVariable_ = 1;
  othernamespace.unusedVariable_ = 1;

  this.element_ = 1;
  this.element_.modifyPublicMember = 1;

  /** @suppress {underscore} */
  this.suppressedUnderscore_ = true;
};


/**
 * Use all the normal variables.
 */
dummy.Something.prototype.useAllTheThings = function() {
  var x = [
    dummy.Something.NORMAL_CONSTANT_,
    this.normalStaticVariable_,
    this.normalVariable_,
    this.normalVariableOnPrototype_,
    dummy.Something.normalStaticMethod_(),
    this.normalMethod_(),
    x
  ];
};


// +5: UNUSED_PRIVATE_MEMBER
/**
 * Unused static method.
 * @private
 */
dummy.Something.unusedStaticMethod_ = function() {
  // Do nothing.
};


/**
 * Unused static method.
 * @private
 * @suppress {unusedPrivateMembers}
 */
dummy.Something.suppressedUnusedStaticMethod_ = function() {
  // Do nothing.
};


/**
 * Normal static method.
 * @private
 */
dummy.Something.normalStaticMethod_ = function() {
  // Do nothing.
};


// +5: UNUSED_PRIVATE_MEMBER
/**
 * Unused non-static method.
 * @private
 */
dummy.Something.prototype.unusedMethod_ = function() {
  // Do nothing.
};


/**
 * Unused non-static method that is suppressed.
 * @private
 * @suppress {unusedPrivateMembers}
 */
dummy.Something.prototype.suppressedUnusedMethod_ = function() {
  // Do nothing.
};


/**
 * Normal non-static method.
 * @private
 */
dummy.Something.prototype.normalMethod_ = function() {
  // Do nothing.
};
