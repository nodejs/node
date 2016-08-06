/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:footnote
 * @fileoverview Stringify a footnote.
 */

'use strict';

/* Expose. */
module.exports = footnote;

/**
 * Stringify a footnote.
 *
 * @param {Object} node - `footnote` node.
 * @return {string} - Markdown footnote.
 */
function footnote(node) {
  return '[^' + this.all(node).join('') + ']';
}
