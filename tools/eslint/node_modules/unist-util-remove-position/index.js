/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module unist:util:remove-position
 * @fileoverview Remove `position`s from a unist tree.
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var visit = require('unist-util-visit');

/* Expose. */
module.exports = removePosition;

/**
 * Remove `position`s from `tree`.
 *
 * @param {Node} tree - Node.
 * @return {Node} - Node without `position`s.
 */
function removePosition(node, force) {
  visit(node, force ? hard : soft);
  return node;
}

/**
 * Delete `position`.
 */
function hard(node) {
  delete node.position;
}

/**
 * Remove `position` softly.
 */
function soft(node) {
  node.position = undefined;
}
