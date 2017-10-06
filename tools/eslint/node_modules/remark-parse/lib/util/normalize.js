/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:util:normalize
 * @fileoverview Normalize an identifier.
 */

'use strict';

/* Dependencies. */
var collapseWhiteSpace = require('collapse-white-space');

/* Expose. */
module.exports = normalize;

/**
 * Normalize an identifier.  Collapses multiple white space
 * characters into a single space, and removes casing.
 *
 * @example
 *   normalizeIdentifier('FOO\t bar'); // 'foo bar'
 *
 * @param {string} value - Content to normalize.
 * @return {string} - Normalized content.
 */
function normalize(value) {
  return collapseWhiteSpace(value).toLowerCase();
}
