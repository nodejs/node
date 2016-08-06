/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenize:html-block
 * @fileoverview Tokenise block HTML.
 */

'use strict';

/* Dependencies. */
var cdata = require('../util/match-cdata');
var comment = require('../util/match-comment');
var declaration = require('../util/match-declaration');
var instruction = require('../util/match-instruction');
var closing = require('../util/match-tag-closing');
var opening = require('../util/match-tag-opening');

/* Expose. */
module.exports = blockHTML;

/* Characters. */
var C_TAB = '\t';
var C_SPACE = ' ';
var C_NEWLINE = '\n';

/* Constants. */
var MIN_CLOSING_HTML_NEWLINE_COUNT = 2;

/**
 * Tokenise block HTML.
 *
 * @param {function(string)} eat - Eater.
 * @param {string} value - Rest of content.
 * @param {boolean?} [silent] - Whether this is a dry run.
 * @return {Node?|boolean} - `html` node.
 */
function blockHTML(eat, value, silent) {
  var self = this;
  var index = 0;
  var length = value.length;
  var subvalue = '';
  var offset;
  var character;
  var queue;

  /* Eat initial spacing. */
  while (index < length) {
    character = value.charAt(index);

    if (character !== C_TAB && character !== C_SPACE) {
      break;
    }

    subvalue += character;
    index++;
  }

  offset = index;
  value = value.slice(offset);

  /* Try to eat an HTML thing. */
  queue = comment(value, self.options) ||
    cdata(value) ||
    instruction(value) ||
    declaration(value) ||
    closing(value, true) ||
    opening(value, true);

  if (!queue) {
    return;
  }

  if (silent) {
    return true;
  }

  subvalue += queue;
  index = subvalue.length - offset;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (character === C_NEWLINE) {
      queue += character;
    } else if (queue.length < MIN_CLOSING_HTML_NEWLINE_COUNT) {
      subvalue += queue + character;
      queue = '';
    } else {
      break;
    }

    index++;
  }

  return eat(subvalue)({
    type: 'html',
    value: subvalue
  });
}
