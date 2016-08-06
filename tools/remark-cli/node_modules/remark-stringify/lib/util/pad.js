/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:util:pad
 * @fileoverview Pad a given value.
 */

'use strict';

/* Dependencies. */
var repeat = require('repeat-string');

/* Expose. */
module.exports = pad;

/* Constants. */
var INDENT = 4;

/**
 * Pad `value` with `level * INDENT` spaces.  Respects
 * lines. Ignores empty lines.
 *
 * @example
 *   pad('foo', 1) // '    foo'
 *
 * @param {string} value - Content.
 * @param {number} level - Indentation level.
 * @return {string} - Padded `value`.
 */
function pad(value, level) {
  var index;
  var padding;

  value = value.split('\n');

  index = value.length;
  padding = repeat(' ', level * INDENT);

  while (index--) {
    if (value[index].length !== 0) {
      value[index] = padding + value[index];
    }
  }

  return value.join('\n');
}
