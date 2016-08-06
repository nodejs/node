/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module unist:util:stringify-position
 * @fileoverview Stringify a Unist node, location, or position.
 */

'use strict';

/* eslint-env commonjs */

/*
 * Methods.
 */

var has = Object.prototype.hasOwnProperty;

/**
 * Stringify a single index.
 *
 * @param {*} value - Index?
 * @return {string?} - Stringified index?
 */
function index(value) {
    return value && typeof value === 'number' ? value : 1;
}

/**
 * Stringify a single position.
 *
 * @param {*} pos - Position?
 * @return {string?} - Stringified position?
 */
function position(pos) {
    if (!pos || typeof pos !== 'object') {
        pos = {};
    }

    return index(pos.line) + ':' + index(pos.column);
}

/**
 * Stringify a single location.
 *
 * @param {*} loc - Location?
 * @return {string?} - Stringified location?
 */
function location(loc) {
    if (!loc || typeof loc !== 'object') {
        loc = {};
    }

    return position(loc.start) + '-' + position(loc.end);
}

/**
 * Stringify a node, location, or position into a range or
 * a point.
 *
 * @param {Node|Position|Location} value - Thing to stringify.
 * @return {string?} - Stringified positional information?
 */
function stringify(value) {
    /* Nothing. */
    if (!value || typeof value !== 'object') {
        return null;
    }

    /* Node. */
    if (has.call(value, 'position') || has.call(value, 'type')) {
        return location(value.position);
    }

    /* Location. */
    if (has.call(value, 'start') || has.call(value, 'end')) {
        return location(value);
    }

    /* Position. */
    if (has.call(value, 'line') || has.call(value, 'column')) {
        return position(value);
    }

    /* ? */
    return null;
}

/*
 * Expose.
 */

module.exports = stringify;
