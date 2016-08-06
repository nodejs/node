/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:strong
 * @fileoverview Stringify a strong.
 */

'use strict';

/* Dependencies. */
var repeat = require('repeat-string');

/* Expose. */
module.exports = strong;

/**
 * Stringify a `strong`.
 *
 * The marker used is configurable by `strong`, which
 * defaults to an asterisk (`'*'`) but also accepts an
 * underscore (`'_'`):
 *
 *     __foo__
 *
 * @param {Object} node - `strong` node.
 * @return {string} - Markdown strong.
 */
function strong(node) {
  var marker = repeat(this.options.strong, 2);
  return marker + this.all(node).join('') + marker;
}
