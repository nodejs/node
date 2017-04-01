/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:util:match-instruction
 * @fileoverview Match XML processing instruction.
 */

'use strict';

/* Expose. */
module.exports = match;

/* Constants. */
var C_QUESTION_MARK = '?';
var C_LT = '<';
var C_GT = '>';

/**
 * Try to match a processing instruction.
 *
 * @param {string} value - Value to parse.
 * @return {string?} - When applicable, the processing
 *   instruction at the start of `value`.
 */
function match(value) {
  var index = 0;
  var queue = '';
  var length = value.length;
  var character;

  if (
    value.charAt(index) === C_LT &&
    value.charAt(++index) === C_QUESTION_MARK
  ) {
    queue = C_LT + C_QUESTION_MARK;
    index++;

    while (index < length) {
      character = value.charAt(index);

      if (
        character === C_QUESTION_MARK &&
        value.charAt(index + 1) === C_GT
      ) {
        return queue + character + C_GT;
      }

      queue += character;
      index++;
    }
  }
}
