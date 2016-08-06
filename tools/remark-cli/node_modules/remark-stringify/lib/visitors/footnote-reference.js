/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:footnote-reference
 * @fileoverview Stringify a footnote reference.
 */

'use strict';

/* Expose. */
module.exports = footnoteReference;

/**
 * Stringify a footnote reference.
 *
 * @param {Object} node - `footnoteReference` node.
 * @return {string} - Markdown footnote reference.
 */
function footnoteReference(node) {
  return '[^' + node.identifier + ']';
}
