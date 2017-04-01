/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:util:match-comment
 * @fileoverview Match XML character data.
 */

'use strict';

/* Expose. */
module.exports = match;

/* Constants. */
var START = '<![CDATA[';
var END = ']]>';
var END_CHAR = END.charAt(0);
var START_LENGTH = START.length;
var END_LENGTH = END.length;

/**
 * Try to match CDATA.
 *
 * @param {string} value - Value to parse.
 * @return {string?} - When applicable, the CDATA at the
 *   start of `value`.
 */
function match(value) {
  var index = START_LENGTH;
  var queue = value.slice(0, index);
  var length = value.length;
  var character;

  if (queue.toUpperCase() === START) {
    while (index < length) {
      character = value.charAt(index);

      if (
        character === END_CHAR &&
        value.slice(index, index + END_LENGTH) === END
      ) {
        return queue + END;
      }

      queue += character;
      index++;
    }
  }
}
