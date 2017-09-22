/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:text
 * @fileoverview Stringify a text.
 */

'use strict';

/* Expose. */
module.exports = text;

/**
 * Stringify text.
 *
 * Supports named entities in `settings.encode: true` mode:
 *
 *     AT&amp;T
 *
 * Supports numbered entities in `settings.encode: numbers`
 * mode:
 *
 *     AT&#x26;T
 *
 * @param {Object} node - `text` node.
 * @param {Object?} [parent] - Parent of `node`.
 * @return {string} - Markdown text.
 */
function text(node, parent) {
  return this.encode(this.escape(node.value, node, parent), node);
}
