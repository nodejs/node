/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:util:match-tag-closing
 * @fileoverview Match an HTML closing tag.
 */

'use strict';

/* Dependencies. */
var alphabetical = require('is-alphabetical');
var decimal = require('is-decimal');
var whitespace = require('is-whitespace-character');
var blockElements = require('../block-elements.json');

/* Expose. */
module.exports = match;

/* Constants. */
var C_LT = '<';
var C_GT = '>';
var C_SLASH = '/';

/**
 * Try to match a closing tag.
 *
 * @param {string} value - Value to parse.
 * @param {boolean?} [isBlock] - Whether the tag-name
 *   must be a known block-level node to match.
 * @return {string?} - When applicable, the closing tag at
 *   the start of `value`.
 */
function match(value, isBlock) {
  var index = 0;
  var length = value.length;
  var queue = '';
  var subqueue = '';
  var character;

  if (
    value.charAt(index) === C_LT &&
    value.charAt(++index) === C_SLASH
  ) {
    queue = C_LT + C_SLASH;
    subqueue = character = value.charAt(++index);

    if (!alphabetical(character)) {
      return;
    }

    index++;

    /* Eat as many alphabetic characters as
     * possible. */
    while (index < length) {
      character = value.charAt(index);

      if (!alphabetical(character) && !decimal(character)) {
        break;
      }

      subqueue += character;
      index++;
    }

    if (isBlock && blockElements.indexOf(subqueue.toLowerCase()) === -1) {
      return;
    }

    queue += subqueue;

    /* Eat white-space. */
    while (index < length) {
      character = value.charAt(index);

      if (!whitespace(character)) {
        break;
      }

      queue += character;
      index++;
    }

    if (value.charAt(index) === C_GT) {
      return queue + C_GT;
    }
  }
}
