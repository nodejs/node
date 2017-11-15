/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:root
 * @fileoverview Stringify a root.
 */

'use strict';

/* Expose. */
module.exports = root;

/**
 * Stringify a root.
 *
 * Adds a final newline to ensure valid POSIX files.
 *
 * @param {Object} node - `root` node.
 * @return {string} - Document.
 */
function root(node) {
  return this.block(node) + '\n';
}
