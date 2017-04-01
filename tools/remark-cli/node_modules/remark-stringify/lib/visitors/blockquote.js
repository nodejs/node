/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:blockquote
 * @fileoverview Stringify a blockquote.
 */

'use strict';

/* Expose. */
module.exports = blockquote;

/**
 * Stringify a blockquote.
 *
 * @param {Object} node - `blockquote` node.
 * @return {string} - Markdown blockquote.
 */
function blockquote(node) {
  var values = this.block(node).split('\n');
  var result = [];
  var length = values.length;
  var index = -1;
  var value;

  while (++index < length) {
    value = values[index];
    result[index] = (value ? ' ' : '') + value;
  }

  return '>' + result.join('\n>');
}
