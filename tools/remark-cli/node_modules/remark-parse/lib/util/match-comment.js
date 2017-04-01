/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:util:match-comment
 * @fileoverview Match an HTML comment.
 */

'use strict';

/* Expose. */
module.exports = match;

/* Constants. */
var START = '<!--';
var END = '-->';
var END_CHAR = END.charAt(0);
var START_LENGTH = START.length;
var END_LENGTH = END.length;

/**
 * Try to match comment.
 *
 * @param {string} value - Value to parse.
 * @param {Object} settings - Configuration as available on
 *   a parser.
 * @return {string?} - When applicable, the comment at the
 *   start of `value`.
 */
function match(value, settings) {
  var index = START_LENGTH;
  var queue = START;
  var length = value.length;
  var commonmark = settings.commonmark;
  var character;
  var hasNonDash;

  if (value.slice(0, index) === queue) {
    while (index < length) {
      character = value.charAt(index);

      if (
        character === END_CHAR &&
        value.slice(index, index + END_LENGTH) === END
      ) {
        return queue + END;
      }

      if (commonmark) {
        if (character === '>' && !hasNonDash) {
          return;
        }

        if (character !== '-') {
          hasNonDash = true;
        } else if (value.charAt(index + 1) === '-') {
          return;
        }
      }

      queue += character;
      index++;
    }
  }
}
