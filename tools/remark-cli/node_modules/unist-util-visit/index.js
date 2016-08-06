/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module unist:util:visit
 * @fileoverview Recursively walk over unist nodes.
 */

'use strict';

/* eslint-env commonjs */

/**
 * Visit.
 *
 * @param {Node} tree - Root node
 * @param {string} [type] - Node type.
 * @param {function(node): boolean?} visitor - Invoked
 *   with each found node.  Can return `false` to stop.
 * @param {boolean} [reverse] - By default, `visit` will
 *   walk forwards, when `reverse` is `true`, `visit`
 *   walks backwards.
 */
function visit(tree, type, visitor, reverse) {
    if (typeof type === 'function') {
        reverse = visitor;
        visitor = type;
        type = null;
    }

    /**
     * Visit children in `parent`.
     *
     * @param {Array.<Node>} children - Children of `node`.
     * @param {Node?} parent - Parent of `node`.
     * @return {boolean?} - `false` if the visiting stopped.
     */
    function all(children, parent) {
        var step = reverse ? -1 : 1;
        var max = children.length;
        var min = -1;
        var index = (reverse ? max : min) + step;
        var child;

        while (index > min && index < max) {
            child = children[index];

            if (child && one(child, index, parent) === false) {
                return false;
            }

            index += step;
        }

        return true;
    }

    /**
     * Visit a single node.
     *
     * @param {Node} node - Node to visit.
     * @param {number?} [index] - Position of `node` in `parent`.
     * @param {Node?} [parent] - Parent of `node`.
     * @return {boolean?} - A result of invoking `visitor`.
     */
    function one(node, index, parent) {
        var result;

        index = index || (parent ? 0 : null);

        if (!type || node.type === type) {
            result = visitor(node, index, parent || null);
        }

        if (node.children && result !== false) {
            return all(node.children, node);
        }

        return result;
    }

    one(tree);
}

/*
 * Expose.
 */

module.exports = visit;
