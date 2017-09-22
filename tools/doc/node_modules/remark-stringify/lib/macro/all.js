/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:macro:all
 * @fileoverview Stringify children in a node.
 */

'use strict';

/* Expose. */
module.exports = all;

/**
 * Visit all children of `parent`.
 *
 * @param {Object} parent - Parent node of children.
 * @return {Array.<string>} - List of compiled children.
 */
function all(parent) {
  var self = this;
  var children = parent.children;
  var length = children.length;
  var results = [];
  var index = -1;

  while (++index < length) {
    results[index] = self.visit(children[index], parent);
  }

  return results;
}
