/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module is-whitespace-character
 * @fileoverview Check if a character is a whitespace character.
 */

'use strict';

/* eslint-env commonjs */

/* Expose. */
module.exports = whitespace;

/* Methods. */
var fromCode = String.fromCharCode;

/* Constants. */
var re = /\s/;

/**
 * Check whether the given character code, or the character
 * code at the first character, is a whitespace character.
 *
 * @param {string|number} character
 * @return {boolean} - Whether `character` is a whitespaces character.
 */
function whitespace(character) {
  return re.test(
    typeof character === 'number' ? fromCode(character) : character.charAt(0)
  );
}
