/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenize:escape
 * @fileoverview Tokenise an escape.
 */

'use strict';

/* Dependencies. */
var locate = require('../locate/escape');

/* Expose. */
module.exports = escape;
escape.locator = locate;

/**
 * Tokenise an escape.
 *
 * @property {Function} locator.
 * @param {function(string)} eat - Eater.
 * @param {string} value - Rest of content.
 * @param {boolean?} [silent] - Whether this is a dry run.
 * @return {Node?|boolean} - `text` or `break` node.
 */
function escape(eat, value, silent) {
  var self = this;
  var character;
  var node;

  if (value.charAt(0) === '\\') {
    character = value.charAt(1);

    if (self.escape.indexOf(character) !== -1) {
      /* istanbul ignore if - never used (yet) */
      if (silent) {
        return true;
      }

      if (character === '\n') {
        node = {type: 'break'};
      } else {
        node = {
          type: 'text',
          value: character
        };
      }

      return eat('\\' + character)(node);
    }
  }
}
