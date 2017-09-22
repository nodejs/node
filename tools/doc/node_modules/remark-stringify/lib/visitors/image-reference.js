/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:image-reference
 * @fileoverview Stringify an image reference.
 */

'use strict';

/* Dependencies. */
var label = require('../util/label');

/* Expose. */
module.exports = imageReference;

/**
 * Stringify an image reference.
 *
 * @param {Object} node - `imageReference` node.
 * @return {string} - Markdown image reference.
 */
function imageReference(node) {
  return '![' + (this.encode(node.alt, node) || '') + ']' + label(node);
}
