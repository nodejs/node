/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:list
 * @fileoverview Stringify a list.
 */

'use strict';

/* Expose. */
module.exports = list;

/* Which method to use based on `list.ordered`. */
var ORDERED_MAP = {
  true: 'visitOrderedItems',
  false: 'visitUnorderedItems'
};

/**
 * Stringify a list. See `Compiler#visitOrderedList()` and
 * `Compiler#visitUnorderedList()` for internal working.
 *
 * @param {Object} node - `list` node.
 * @return {string} - Markdown list.
 */
function list(node) {
  return this[ORDERED_MAP[node.ordered]](node);
}
