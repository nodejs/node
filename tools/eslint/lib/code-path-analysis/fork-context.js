/**
 * @fileoverview A class to operate forking.
 *
 * This is state of forking.
 * This has a fork list and manages it.
 *
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var assert = require("assert"),
    CodePathSegment = require("./code-path-segment");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Gets whether or not a given segment is reachable.
 *
 * @param {CodePathSegment} segment - A segment to get.
 * @returns {boolean} `true` if the segment is reachable.
 */
function isReachable(segment) {
    return segment.reachable;
}

/**
 * Creates new segments from the specific range of `context.segmentsList`.
 *
 * When `context.segmentsList` is `[[a, b], [c, d], [e, f]]`, `begin` is `0`, and
 * `end` is `-1`, this creates `[g, h]`. This `g` is from `a`, `c`, and `e`.
 * This `h` is from `b`, `d`, and `f`.
 *
 * @param {ForkContext} context - An instance.
 * @param {number} begin - The first index of the previous segments.
 * @param {number} end - The last index of the previous segments.
 * @param {function} create - A factory function of new segments.
 * @returns {CodePathSegment[]} New segments.
 */
function makeSegments(context, begin, end, create) {
    var list = context.segmentsList;
    if (begin < 0) {
        begin = list.length + begin;
    }
    if (end < 0) {
        end = list.length + end;
    }

    var segments = [];
    for (var i = 0; i < context.count; ++i) {
        var allPrevSegments = [];

        for (var j = begin; j <= end; ++j) {
            allPrevSegments.push(list[j][i]);
        }

        segments.push(create(context.idGenerator.next(), allPrevSegments));
    }

    return segments;
}

/**
 * `segments` becomes doubly in a `finally` block. Then if a code path exits by a
 * control statement (such as `break`, `continue`) from the `finally` block, the
 * destination's segments may be half of the source segments. In that case, this
 * merges segments.
 *
 * @param {ForkContext} context - An instance.
 * @param {CodePathSegment[]} segments - Segments to merge.
 * @returns {CodePathSegment[]} The merged segments.
 */
function mergeExtraSegments(context, segments) {
    while (segments.length > context.count) {
        var merged = [];
        for (var i = 0, length = segments.length / 2 | 0; i < length; ++i) {
            merged.push(CodePathSegment.newNext(
                context.idGenerator.next(),
                [segments[i], segments[i + length]]
            ));
        }
        segments = merged;
    }
    return segments;
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * A class to manage forking.
 *
 * @constructor
 * @param {IdGenerator} idGenerator - An identifier generator for segments.
 * @param {ForkContext|null} upper - An upper fork context.
 * @param {number} count - A number of parallel segments.
 */
function ForkContext(idGenerator, upper, count) {
    this.idGenerator = idGenerator;
    this.upper = upper;
    this.count = count;
    this.segmentsList = [];
}

ForkContext.prototype = {
    constructor: ForkContext,

    /**
     * The head segments.
     * @type {CodePathSegment[]}
     */
    get head() {
        var list = this.segmentsList;
        return list.length === 0 ? [] : list[list.length - 1];
    },

    /**
     * A flag which shows empty.
     * @type {boolean}
     */
    get empty() {
        return this.segmentsList.length === 0;
    },

    /**
     * A flag which shows reachable.
     * @type {boolean}
     */
    get reachable() {
        var segments = this.head;
        return segments.length > 0 && segments.some(isReachable);
    },

    /**
     * Creates new segments from this context.
     *
     * @param {number} begin - The first index of previous segments.
     * @param {number} end - The last index of previous segments.
     * @returns {CodePathSegment[]} New segments.
     */
    makeNext: function(begin, end) {
        return makeSegments(this, begin, end, CodePathSegment.newNext);
    },

    /**
     * Creates new segments from this context.
     * The new segments is always unreachable.
     *
     * @param {number} begin - The first index of previous segments.
     * @param {number} end - The last index of previous segments.
     * @returns {CodePathSegment[]} New segments.
     */
    makeUnreachable: function(begin, end) {
        return makeSegments(this, begin, end, CodePathSegment.newUnreachable);
    },

    /**
     * Creates new segments from this context.
     * The new segments don't have connections for previous segments.
     * But these inherit the reachable flag from this context.
     *
     * @param {number} begin - The first index of previous segments.
     * @param {number} end - The last index of previous segments.
     * @returns {CodePathSegment[]} New segments.
     */
    makeDisconnected: function(begin, end) {
        return makeSegments(this, begin, end, CodePathSegment.newDisconnected);
    },

    /**
     * Adds segments into this context.
     * The added segments become the head.
     *
     * @param {CodePathSegment[]} segments - Segments to add.
     * @returns {void}
     */
    add: function(segments) {
        assert(segments.length >= this.count, segments.length + " >= " + this.count);

        this.segmentsList.push(mergeExtraSegments(this, segments));
    },

    /**
     * Replaces the head segments with given segments.
     * The current head segments are removed.
     *
     * @param {CodePathSegment[]} segments - Segments to add.
     * @returns {void}
     */
    replaceHead: function(segments) {
        assert(segments.length >= this.count, segments.length + " >= " + this.count);

        this.segmentsList.splice(-1, 1, mergeExtraSegments(this, segments));
    },

    /**
     * Adds all segments of a given fork context into this context.
     *
     * @param {ForkContext} context - A fork context to add.
     * @returns {void}
     */
    addAll: function(context) {
        assert(context.count === this.count);

        var source = context.segmentsList;
        for (var i = 0; i < source.length; ++i) {
            this.segmentsList.push(source[i]);
        }
    },

    /**
     * Clears all secments in this context.
     *
     * @returns {void}
     */
    clear: function() {
        this.segmentsList = [];
    }
};

/**
 * Creates the root fork context.
 *
 * @param {IdGenerator} idGenerator - An identifier generator for segments.
 * @returns {ForkContext} New fork context.
 */
ForkContext.newRoot = function(idGenerator) {
    var context = new ForkContext(idGenerator, null, 1);

    context.add([CodePathSegment.newRoot(idGenerator.next())]);

    return context;
};

/**
 * Creates an empty fork context preceded by a given context.
 *
 * @param {ForkContext} parentContext - The parent fork context.
 * @param {boolean} forkLeavingPath - A flag which shows inside of `finally` block.
 * @returns {ForkContext} New fork context.
 */
ForkContext.newEmpty = function(parentContext, forkLeavingPath) {
    return new ForkContext(
        parentContext.idGenerator,
        parentContext,
        (forkLeavingPath ? 2 : 1) * parentContext.count);
};

module.exports = ForkContext;
