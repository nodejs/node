/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module is-decimal
 * @fileoverview Check if a character is decimal.
 */

'use strict';

/* eslint-env commonjs */

/* Expose. */
module.exports = decimal;

/**
 * Check whether the given character code, or the character
 * code at the first character, is decimal.
 *
 * @param {string|number} character
 * @return {boolean} - Whether `character` is decimal.
 */
function decimal(character) {
  var code = typeof character === 'string' ?
    character.charCodeAt(0) : character;

  return code >= 48 && code <= 57; /* 0-9 */
}
