/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenize:definition
 * @fileoverview Tokenise a definition.
 */

'use strict';

/* Dependencies. */
var whitespace = require('is-whitespace-character');
var normalize = require('../util/normalize');

/* Expose. */
module.exports = definition;
definition.notInList = true;
definition.notInBlock = true;

/* Characters */
var C_DOUBLE_QUOTE = '"';
var C_SINGLE_QUOTE = '\'';
var C_BACKSLASH = '\\';
var C_NEWLINE = '\n';
var C_TAB = '\t';
var C_SPACE = ' ';
var C_BRACKET_OPEN = '[';
var C_BRACKET_CLOSE = ']';
var C_PAREN_OPEN = '(';
var C_PAREN_CLOSE = ')';
var C_COLON = ':';
var C_LT = '<';
var C_GT = '>';

/**
 * Tokenise a definition.
 *
 * @param {function(string)} eat - Eater.
 * @param {string} value - Rest of content.
 * @param {boolean?} [silent] - Whether this is a dry run.
 * @return {Node?|boolean} - `definition` node.
 */
function definition(eat, value, silent) {
  var self = this;
  var commonmark = self.options.commonmark;
  var index = 0;
  var length = value.length;
  var subvalue = '';
  var beforeURL;
  var beforeTitle;
  var queue;
  var character;
  var test;
  var identifier;
  var url;
  var title;

  while (index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE && character !== C_TAB) {
      break;
    }

    subvalue += character;
    index++;
  }

  character = value.charAt(index);

  if (character !== C_BRACKET_OPEN) {
    return;
  }

  index++;
  subvalue += character;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (character === C_BRACKET_CLOSE) {
      break;
    } else if (character === C_BACKSLASH) {
      queue += character;
      index++;
      character = value.charAt(index);
    }

    queue += character;
    index++;
  }

  if (
    !queue ||
    value.charAt(index) !== C_BRACKET_CLOSE ||
    value.charAt(index + 1) !== C_COLON
  ) {
    return;
  }

  identifier = queue;
  subvalue += queue + C_BRACKET_CLOSE + C_COLON;
  index = subvalue.length;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (
      character !== C_TAB &&
      character !== C_SPACE &&
      character !== C_NEWLINE
    ) {
      break;
    }

    subvalue += character;
    index++;
  }

  character = value.charAt(index);
  queue = '';
  beforeURL = subvalue;

  if (character === C_LT) {
    index++;

    while (index < length) {
      character = value.charAt(index);

      if (!isEnclosedURLCharacter(character)) {
        break;
      }

      queue += character;
      index++;
    }

    character = value.charAt(index);

    if (character === isEnclosedURLCharacter.delimiter) {
      subvalue += C_LT + queue + character;
      index++;
    } else {
      if (commonmark) {
        return;
      }

      index -= queue.length + 1;
      queue = '';
    }
  }

  if (!queue) {
    while (index < length) {
      character = value.charAt(index);

      if (!isUnclosedURLCharacter(character)) {
        break;
      }

      queue += character;
      index++;
    }

    subvalue += queue;
  }

  if (!queue) {
    return;
  }

  url = queue;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (
      character !== C_TAB &&
      character !== C_SPACE &&
      character !== C_NEWLINE
    ) {
      break;
    }

    queue += character;
    index++;
  }

  character = value.charAt(index);
  test = null;

  if (character === C_DOUBLE_QUOTE) {
    test = C_DOUBLE_QUOTE;
  } else if (character === C_SINGLE_QUOTE) {
    test = C_SINGLE_QUOTE;
  } else if (character === C_PAREN_OPEN) {
    test = C_PAREN_CLOSE;
  }

  if (!test) {
    queue = '';
    index = subvalue.length;
  } else if (queue) {
    subvalue += queue + character;
    index = subvalue.length;
    queue = '';

    while (index < length) {
      character = value.charAt(index);

      if (character === test) {
        break;
      }

      if (character === C_NEWLINE) {
        index++;
        character = value.charAt(index);

        if (character === C_NEWLINE || character === test) {
          return;
        }

        queue += C_NEWLINE;
      }

      queue += character;
      index++;
    }

    character = value.charAt(index);

    if (character !== test) {
      return;
    }

    beforeTitle = subvalue;
    subvalue += queue + character;
    index++;
    title = queue;
    queue = '';
  } else {
    return;
  }

  while (index < length) {
    character = value.charAt(index);

    if (character !== C_TAB && character !== C_SPACE) {
      break;
    }

    subvalue += character;
    index++;
  }

  character = value.charAt(index);

  if (!character || character === C_NEWLINE) {
    if (silent) {
      return true;
    }

    beforeURL = eat(beforeURL).test().end;
    url = self.decode.raw(self.unescape(url), beforeURL);

    if (title) {
      beforeTitle = eat(beforeTitle).test().end;
      title = self.decode.raw(self.unescape(title), beforeTitle);
    }

    return eat(subvalue)({
      type: 'definition',
      identifier: normalize(identifier),
      title: title || null,
      url: url
    });
  }
}

/**
 * Check whether `character` can be inside an enclosed
 * URI.
 *
 * @property {string} delimiter - Closing delimiter.
 * @param {string} character - Character to test.
 * @return {boolean} - Whether `character` can be inside
 *   an enclosed URI.
 */
function isEnclosedURLCharacter(character) {
  return character !== C_GT &&
    character !== C_BRACKET_OPEN &&
    character !== C_BRACKET_CLOSE;
}

isEnclosedURLCharacter.delimiter = C_GT;

/**
 * Check whether `character` can be inside an unclosed
 * URI.
 *
 * @param {string} character - Character to test.
 * @return {boolean} - Whether `character` can be inside
 *   an unclosed URI.
 */
function isUnclosedURLCharacter(character) {
  return character !== C_BRACKET_OPEN &&
    character !== C_BRACKET_CLOSE &&
    !whitespace(character);
}
