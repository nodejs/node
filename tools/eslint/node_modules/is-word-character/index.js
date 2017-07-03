/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module is-word-character
 * @fileoverview Check if a character is a word character.
 */

'use strict';

/* eslint-env commonjs */

/* Expose. */
module.exports = wordCharacter;

/* Methods. */
var fromCode = String.fromCharCode;

/* Constants. */
var re = /\w/;

/**
 * Check whether the given character code, or the character
 * code at the first character, is a word character.
 *
 * @param {string|number} character
 * @return {boolean} - Whether `character` is a word character.
 */
function wordCharacter(character) {
  return re.test(
    typeof character === 'number' ? fromCode(character) : character.charAt(0)
  );
}
