/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:macro:one
 * @fileoverview Stringify a node.
 */

'use strict';

/* Expose. */
module.exports = one;

/**
 * Visit a node.
 *
 * @param {Object} node - Node.
 * @param {Object?} [parent] - `node`s parent.
 * @return {string} - Compiled `node`.
 */
function one(node, parent) {
  var self = this;
  var visitors = self.visitors;

  /* Fail on unknown nodes. */
  if (typeof visitors[node.type] !== 'function') {
    self.file.fail(
      'Missing compiler for node of type `' +
      node.type + '`: `' + node + '`',
      node
    );
  }

  return visitors[node.type].call(self, node, parent);
}
