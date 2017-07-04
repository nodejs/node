/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module is-alphanumerical
 * @fileoverview Check if a character is alphanumerical.
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var alphabetical = require('is-alphabetical');
var decimal = require('is-decimal');

/* Expose. */
module.exports = alphanumerical;

/**
 * Check whether the given character code, or the character
 * code at the first character, is alphanumerical.
 *
 * @param {string|number} character
 * @return {boolean} - Whether `character` is alphanumerical.
 */
function alphanumerical(character) {
  return alphabetical(character) || decimal(character);
}
