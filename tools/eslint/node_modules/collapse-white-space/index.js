/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module collapse-white-space
 * @fileoverview Replace multiple white-space characters
 *   with a single space.
 */

'use strict';

/* Expose. */
module.exports = collapse;

/**
 * Replace multiple white-space characters with a single space.
 *
 * @example
 *   collapse(' \t\nbar \nbaz\t'); // ' bar baz '
 *
 * @param {string} value - Value with uncollapsed white-space,
 *   coerced to string.
 * @return {string} - Value with collapsed white-space.
 */
function collapse(value) {
  return String(value).replace(/\s+/g, ' ');
}
