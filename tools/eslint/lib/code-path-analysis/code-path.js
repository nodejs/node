/**
 * @fileoverview A class of the code path.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var CodePathState = require("./code-path-state");
var IdGenerator = require("./id-generator");

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * A code path.
 *
 * @constructor
 * @param {string} id - An identifier.
 * @param {CodePath|null} upper - The code path of the upper function scope.
 * @param {function} onLooped - A callback function to notify looping.
 */
function CodePath(id, upper, onLooped) {
    /**
     * The identifier of this code path.
     * Rules use it to store additional information of each rule.
     * @type {string}
     */
    this.id = id;

    /**
     * The code path of the upper function scope.
     * @type {CodePath|null}
     */
    this.upper = upper;

    /**
     * The code paths of nested function scopes.
     * @type {CodePath[]}
     */
    this.childCodePaths = [];

    // Initializes internal state.
    Object.defineProperty(
        this,
        "internal",
        {value: new CodePathState(new IdGenerator(id + "_"), onLooped)});

    // Adds this into `childCodePaths` of `upper`.
    if (upper) {
        upper.childCodePaths.push(this);
    }
}

CodePath.prototype = {
    constructor: CodePath,

    /**
     * The initial code path segment.
     * @type {CodePathSegment}
     */
    get initialSegment() {
        return this.internal.initialSegment;
    },

    /**
     * Final code path segments.
     * This array is a mix of `returnedSegments` and `thrownSegments`.
     * @type {CodePathSegment[]}
     */
    get finalSegments() {
        return this.internal.finalSegments;
    },

    /**
     * Final code path segments which is with `return` statements.
     * This array contains the last path segment if it's reachable.
     * Since the reachable last path returns `undefined`.
     * @type {CodePathSegment[]}
     */
    get returnedSegments() {
        return this.internal.returnedForkContext;
    },

    /**
     * Final code path segments which is with `throw` statements.
     * @type {CodePathSegment[]}
     */
    get thrownSegments() {
        return this.internal.thrownForkContext;
    },

    /**
     * Current code path segments.
     * @type {CodePathSegment[]}
     */
    get currentSegments() {
        return this.internal.currentSegments;
    }
};

/**
 * Gets the state of a given code path.
 *
 * @param {CodePath} codePath - A code path to get.
 * @returns {CodePathState} The state of the code path.
 */
CodePath.getState = function getState(codePath) {
    return codePath.internal;
};

module.exports = CodePath;
