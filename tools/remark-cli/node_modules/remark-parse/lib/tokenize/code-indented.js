/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenize:code-indented
 * @fileoverview Tokenise indented code.
 */

'use strict';

/* Dependencies. */
var repeat = require('repeat-string');
var trim = require('trim-trailing-lines');

/* Expose. */
module.exports = indentedCode;

/* Characters */
var C_NEWLINE = '\n';
var C_TAB = '\t';
var C_SPACE = ' ';

/* Constants */
var CODE_INDENT_COUNT = 4;
var CODE_INDENT = repeat(C_SPACE, CODE_INDENT_COUNT);

/**
 * Tokenise indented code.
 *
 * @property {Function} locator.
 * @param {function(string)} eat - Eater.
 * @param {string} value - Rest of content.
 * @param {boolean?} [silent] - Whether this is a dry run.
 * @return {Node?|boolean} - `code` node.
 */
function indentedCode(eat, value, silent) {
  var index = -1;
  var length = value.length;
  var subvalue = '';
  var content = '';
  var subvalueQueue = '';
  var contentQueue = '';
  var character;
  var blankQueue;
  var indent;

  while (++index < length) {
    character = value.charAt(index);

    if (indent) {
      indent = false;

      subvalue += subvalueQueue;
      content += contentQueue;
      subvalueQueue = contentQueue = '';

      if (character === C_NEWLINE) {
        subvalueQueue = contentQueue = character;
      } else {
        subvalue += character;
        content += character;

        while (++index < length) {
          character = value.charAt(index);

          if (!character || character === C_NEWLINE) {
            contentQueue = subvalueQueue = character;
            break;
          }

          subvalue += character;
          content += character;
        }
      }
    } else if (
      character === C_SPACE &&
      value.charAt(index + 1) === character &&
      value.charAt(index + 2) === character &&
      value.charAt(index + 3) === character
    ) {
      subvalueQueue += CODE_INDENT;
      index += 3;
      indent = true;
    } else if (character === C_TAB) {
      subvalueQueue += character;
      indent = true;
    } else {
      blankQueue = '';

      while (character === C_TAB || character === C_SPACE) {
        blankQueue += character;
        character = value.charAt(++index);
      }

      if (character !== C_NEWLINE) {
        break;
      }

      subvalueQueue += blankQueue + character;
      contentQueue += character;
    }
  }

  if (content) {
    if (silent) {
      return true;
    }

    return eat(subvalue)({
      type: 'code',
      lang: null,
      value: trim(content)
    });
  }
}
