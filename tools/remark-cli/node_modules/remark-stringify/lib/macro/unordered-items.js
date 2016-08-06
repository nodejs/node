/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:macro:unordered-items
 * @fileoverview Stringify unordered list items.
 */

'use strict';

/* Expose. */
module.exports = unorderedItems;

/**
 * Visit unordered list items.
 *
 * Uses `options.bullet` as each item's bullet.
 *
 * @param {Object} node - `list` node with
 *   `ordered: false`.
 * @return {string} - Compiled children.
 */
function unorderedItems(node) {
  var self = this;
  var bullet = self.options.bullet;
  var fn = self.visitors.listItem;
  var children = node.children;
  var length = children.length;
  var index = -1;
  var values = [];

  while (++index < length) {
    values[index] = fn.call(self, children[index], node, index, bullet);
  }

  return values.join('\n');
}
