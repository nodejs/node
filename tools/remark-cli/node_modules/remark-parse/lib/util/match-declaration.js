/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:util:match-declaration
 * @fileoverview Match an XML declaration.
 */

'use strict';

/* Dependencies. */
var alphabetical = require('is-alphabetical');
var whitespace = require('is-whitespace-character');

/* Expose. */
module.exports = match;

/* Constants. */
var C_EXCLAMATION_MARK = '!';
var C_LT = '<';
var C_GT = '>';

/**
 * Try to match a declaration.
 *
 * @param {string} value - Value to parse.
 * @return {string?} - When applicable, the declaration at
 *   the start of `value`.
 */
function match(value) {
  var index = 0;
  var length = value.length;
  var queue = '';
  var subqueue = '';
  var character;

  if (
    value.charAt(index) === C_LT &&
    value.charAt(++index) === C_EXCLAMATION_MARK
  ) {
    queue = C_LT + C_EXCLAMATION_MARK;
    index++;

    /* Eat as many alphabetic characters as
     * possible. */
    while (index < length) {
      character = value.charAt(index);

      if (!alphabetical(character)) {
        break;
      }

      subqueue += character;
      index++;
    }

    character = value.charAt(index);

    if (!subqueue || !whitespace(character)) {
      return;
    }

    queue += subqueue + character;
    index++;

    while (index < length) {
      character = value.charAt(index);

      if (character === C_GT) {
        return queue;
      }

      queue += character;
      index++;
    }
  }
}
