/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module unist:util:position
 * @fileoverview Utility to get either the starting or the
 *   ending position of a node, and if its generated or not.
 */

'use strict';

/* eslint-env commonjs */

/**
 * Factory to get a position at `type`.
 *
 * @example
 *   positionFactory('start'); // Function
 *
 *   positionFactory('end'); // Function
 *
 * @param {string} type - Either `'start'` or `'end'`.
 * @return {function(Node): Object} - Getter.
 */
function positionFactory(type) {
    /**
     * Get a position in `node` at a bound `type`.
     *
     * @example
     *   // When bound to `start`.
     *   start({
     *     start: {
     *       line: 1,
     *       column: 1
     *     }
     *   }); // {line: 1, column: 1}
     *
     *   // When bound to `end`.
     *   end({
     *     end: {
     *       line: 1,
     *       column: 2
     *     }
     *   }); // {line: 1, column: 2}
     *
     * @param {Node} node - Node to check.
     * @return {Object} - Position at `type` in `node`, or
     *   an empty object.
     */
    return function (node) {
        var pos = (node && node.position && node.position[type]) || {};

        return {
            'line': pos.line || null,
            'column': pos.column || null,
            'offset': isNaN(pos.offset) ? null : pos.offset
        };
    };
}

/*
 * Getters.
 */

var position = {
    'start': positionFactory('start'),
    'end': positionFactory('end')
};

/**
 * Detect if a node was available in the original document.
 *
 * @example
 *   generated(); // true
 *
 *   generated({
 *     start: {
 *       line: 1,
 *       column: 1
 *     },
 *     end: {
 *       line: 1,
 *       column: 2
 *     }
 *   }); // false
 *
 * @param {Node} node - Node to test.
 * @return {boolean} - Whether or not `node` is generated.
 */
function generated(node) {
    var initial = position.start(node);
    var final = position.end(node);

    return initial.line === null || initial.column === null ||
        final.line === null || final.column === null;
}

/*
 * Expose.
 */

position.generated = generated;

module.exports = position;
