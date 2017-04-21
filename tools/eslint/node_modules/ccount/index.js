/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module ccount
 * @fileoverview Count characters.
 */

'use strict';

/* Expose. */
module.exports = ccount;

/**
 * Count how many characters `character` occur in `value`.
 *
 * @example
 *   ccount('foo(bar(baz)', '(') // 2
 *   ccount('foo(bar(baz)', ')') // 1
 *
 * @param {string} value - Content, coerced to string.
 * @param {string} character - Single character to look
 *   for.
 * @return {number} - Count.
 * @throws {Error} - when `character` is not a single
 *   character.
 */
function ccount(value, character) {
  var count = 0;
  var index;

  value = String(value);

  if (typeof character !== 'string' || character.length !== 1) {
    throw new Error('Expected character');
  }

  index = value.indexOf(character);

  while (index !== -1) {
    count++;
    index = value.indexOf(character, index + 1);
  }

  return count;
}
