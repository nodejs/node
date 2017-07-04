/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module is-alphabetical
 * @fileoverview Check if a character is alphabetical.
 */

'use strict';

/* eslint-env commonjs */

/* Expose. */
module.exports = alphabetical;

/**
 * Check whether the given character code, or the character
 * code at the first character, is alphabetical.
 *
 * @param {string|number} character
 * @return {boolean} - Whether `character` is alphabetical.
 */
function alphabetical(character) {
  var code = typeof character === 'string' ?
    character.charCodeAt(0) : character;

  return (code >= 97 && code <= 122) || /* a-z */
    (code >= 65 && code <= 90); /* A-Z */
}
