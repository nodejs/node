/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:macro:compile
 * @fileoverview Compile the given node.
 */

'use strict';

/* Dependencies. */
var compact = require('mdast-util-compact');

/* Expose. */
module.exports = compile;

/**
 * Stringify the given tree.
 *
 * @param {Node} node - Syntax tree.
 * @return {string} - Markdown document.
 */
function compile(node) {
  return this.visit(compact(node, this.options.commonmark));
}
