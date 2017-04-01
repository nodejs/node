/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module unist:util:modify-children
 * @fileoverview Unist utility to modify direct children of a parent.
 */

'use strict';

/* eslint-env commonjs */

/*
 * Dependencies.
 */

var iterate = require('array-iterate');

/**
 * Modifier for children of `parent`.
 *
 * @typedef modifyChildren~callback
 * @param {Node} child - Current iteration;
 * @param {number} index - Position of `child` in `parent`;
 * @param {Node} parent - Parent node of `child`.
 * @return {number?} - Next position to iterate.
 */

/**
 * Function invoking a bound `fn` for each child of `parent`.
 *
 * @typedef modifyChildren~modifier
 * @param {Node} parent - Node with children.
 * @throws {Error} - When not given a parent node.
 */

/**
 * Pass the context as the third argument to `callback`.
 *
 * @param {modifyChildren~callback} callback - Function to wrap.
 * @return {function(Node, number): number?} - Intermediate
 *   version partially aplied version of
 *   `modifyChildren~modifier`.
 */
function wrapperFactory(callback) {
    return function (value, index) {
        return callback(value, index, this);
    };
}

/**
 * Turns `callback` into a ``iterator'' accepting a parent.
 *
 * see ``array-iterate'' for more info.
 *
 * @param {modifyChildren~callback} callback - Function to wrap.
 * @return {modifyChildren~modifier}
 */
function iteratorFactory(callback) {
    return function (parent) {
        var children = parent && parent.children;

        if (!children) {
            throw new Error('Missing children in `parent` for `modifier`');
        }

        return iterate(children, callback, parent);
    };
}

/**
 * Turns `callback` into a child-modifier accepting a parent.
 *
 * See `array-iterate` for more info.
 *
 * @param {modifyChildren~callback} callback - Function to wrap.
 * @return {modifyChildren~modifier} - Wrapped `fn`.
 */
function modifierFactory(callback) {
    return iteratorFactory(wrapperFactory(callback));
}

/*
 * Expose.
 */

module.exports = modifierFactory;
