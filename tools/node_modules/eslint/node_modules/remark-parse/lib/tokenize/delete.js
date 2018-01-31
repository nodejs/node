/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenize:delete
 * @fileoverview Tokenise strikethrough.
 */

'use strict';

var whitespace = require('is-whitespace-character');
var locate = require('../locate/delete');

module.exports = strikethrough;
strikethrough.locator = locate;

var C_TILDE = '~';
var DOUBLE = '~~';

/* Tokenise strikethrough. */
function strikethrough(eat, value, silent) {
  var self = this;
  var character = '';
  var previous = '';
  var preceding = '';
  var subvalue = '';
  var index;
  var length;
  var now;

  if (
    !self.options.gfm ||
    value.charAt(0) !== C_TILDE ||
    value.charAt(1) !== C_TILDE ||
    whitespace(value.charAt(2))
  ) {
    return;
  }

  index = 1;
  length = value.length;
  now = eat.now();
  now.column += 2;
  now.offset += 2;

  while (++index < length) {
    character = value.charAt(index);

    if (
      character === C_TILDE &&
      previous === C_TILDE &&
      (!preceding || !whitespace(preceding))
    ) {
      /* istanbul ignore if - never used (yet) */
      if (silent) {
        return true;
      }

      return eat(DOUBLE + subvalue + DOUBLE)({
        type: 'delete',
        children: self.tokenizeInline(subvalue, now)
      });
    }

    subvalue += previous;
    preceding = previous;
    previous = character;
  }
}
