/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module mdast:util:to-string
 * @fileoverview Utility to get the plain text content of a node.
 */

'use strict';

/* eslint-env node */

/**
 * Get the value of `node`.  Checks, `value`,
 * `alt`, and `title`, in that order.
 *
 * @param {Node} node - Node to get the internal value of.
 * @return {string} - Textual representation.
 */
function valueOf(node) {
    return node &&
        (node.value ? node.value :
        (node.alt ? node.alt : node.title)) || '';
}

/**
 * Returns the text content of a node.  If the node itself
 * does not expose plain-text fields, `toString` will
 * recursivly try its children.
 *
 * @param {Node} node - Node to transform to a string.
 * @return {string} - Textual representation.
 */
function toString(node) {
    return valueOf(node) ||
        (node.children && node.children.map(toString).join('')) ||
        '';
}

/*
 * Expose.
 */

module.exports = toString;
