// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
function InjectedScriptHostClass()
{
}

/**
 * @param {*} objectId
 * @param {!Object} hints
 */
InjectedScriptHostClass.prototype.inspect = function(objectId, hints) {}

/**
 * @param {*} obj
 * @return {string}
 */
InjectedScriptHostClass.prototype.internalConstructorName = function(obj) {}

/**
 * @param {*} obj
 * @return {boolean}
 */
InjectedScriptHostClass.prototype.formatAccessorsAsProperties = function(obj) {}

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
 * @param {!Object} obj
 * @return {?Array.<*>}
 */
InjectedScriptHostClass.prototype.collectionEntries = function(obj) {}

/**
 * @param {*} obj
 * @return {!Array.<*>}
 */
InjectedScriptHostClass.prototype.getInternalProperties = function(obj) {}

/**
 * @param {!EventTarget} target
 * @return {!Object|undefined}
 */
InjectedScriptHostClass.prototype.getEventListeners = function(target) {}

/**
 * @param {!Function} fn
 * @param {*} receiver
 * @param {!Array.<*>=} argv
 * @return {*}
 */
InjectedScriptHostClass.prototype.suppressWarningsAndCallFunction = function(fn, receiver, argv) {}

/**
 * @param {!Object} obj
 * @param {string} key
 * @param {*} value
 */
InjectedScriptHostClass.prototype.setNonEnumProperty = function(obj, key, value) {}

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

/**
 * @param {!Object} object
 * @return {Object|undefined}
 */
InjectedScriptHostClass.prototype.prototype = function(object) {}

/** @type {!InjectedScriptHostClass} */
var InjectedScriptHost;
/** @type {!Window} */
var inspectedGlobalObject;
/** @type {number} */
var injectedScriptId;
