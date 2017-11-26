/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:delete
 * @fileoverview Stringify a delete.
 */

'use strict';

/* Expose. */
module.exports = strikethrough;

/**
 * Stringify a `delete`.
 *
 * @param {Object} node - `delete` node.
 * @return {string} - Markdown strikethrough.
 */
function strikethrough(node) {
  return '~~' + this.all(node).join('') + '~~';
}
