// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
function InjectedScriptHostClass()
{
}

/**
 * @param {*} obj
 * @return {string}
 */
InjectedScriptHostClass.prototype.internalConstructorName = function(obj) {}

/**
 * @param {*} obj
 * @param {function()|undefined} func
 * @return {boolean}
 */
InjectedScriptHostClass.prototype.formatAccessorsAsProperties = function(obj, func) {}

/**
 * @param {*} obj
 * @return {string}
 */
InjectedScriptHostClass.prototype.subtype = function(obj) {}

/**
 * @param {*} obj
 * @return {boolean}
 */
InjectedScriptHostClass.prototype.isTypedArray = function(obj) {}

/**
 * @param {*} obj
 * @return {!Array.<*>}
 */
InjectedScriptHostClass.prototype.getInternalProperties = function(obj) {}

/**
 * @param {!Object} object
 * @param {string} propertyName
 * @return {boolean}
 */
InjectedScriptHostClass.prototype.objectHasOwnProperty = function(object, propertyName) {}

/**
 * @param {*} value
 * @param {string} groupName
 * @return {number}
 */
InjectedScriptHostClass.prototype.bind = function(value, groupName) {}

/**
 * @param {!Object} object
 * @return {!Object}
 */
InjectedScriptHostClass.prototype.proxyTargetValue = function(object) {}

/** @type {!InjectedScriptHostClass} */
var InjectedScriptHost;
/** @type {!Window} */
var inspectedGlobalObject;
/** @type {number} */
var injectedScriptId;
