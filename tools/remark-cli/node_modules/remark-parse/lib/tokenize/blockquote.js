/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenize:blockquote
 * @fileoverview Tokenise blockquote.
 */

'use strict';

/* Dependencies. */
var trim = require('trim');

/* Expose. */
module.exports = blockquote;

/* Characters */
var C_NEWLINE = '\n';
var C_TAB = '\t';
var C_SPACE = ' ';
var C_GT = '>';

/**
 * Tokenise a blockquote.
 *
 * @property {Function} locator.
 * @param {function(string)} eat - Eater.
 * @param {string} value - Rest of content.
 * @param {boolean?} [silent] - Whether this is a dry run.
 * @return {Node?|boolean} - `blockquote` node.
 */
function blockquote(eat, value, silent) {
  var self = this;
  var commonmark = self.options.commonmark;
  var offsets = self.offset;
  var now = eat.now();
  var currentLine = now.line;
  var length = value.length;
  var values = [];
  var contents = [];
  var indents = [];
  var add;
  var tokenizers;
  var index = 0;
  var character;
  var rest;
  var nextIndex;
  var content;
  var line;
  var startIndex;
  var prefixed;
  var exit;

  while (index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE && character !== C_TAB) {
      break;
    }

    index++;
  }

  if (value.charAt(index) !== C_GT) {
    return;
  }

  if (silent) {
    return true;
  }

  tokenizers = self.blockTokenizers;
  index = 0;

  while (index < length) {
    nextIndex = value.indexOf(C_NEWLINE, index);
    startIndex = index;
    prefixed = false;

    if (nextIndex === -1) {
      nextIndex = length;
    }

    while (index < length) {
      character = value.charAt(index);

      if (character !== C_SPACE && character !== C_TAB) {
        break;
      }

      index++;
    }

    if (value.charAt(index) === C_GT) {
      index++;
      prefixed = true;

      if (value.charAt(index) === C_SPACE) {
        index++;
      }
    } else {
      index = startIndex;
    }

    content = value.slice(index, nextIndex);

    if (!prefixed && !trim(content)) {
      index = startIndex;
      break;
    }

    if (!prefixed) {
      rest = value.slice(index);

      if (
        (
          commonmark &&
          (
            tokenizers.indentedCode.call(self, eat, rest, true) ||
            tokenizers.fencedCode.call(self, eat, rest, true) ||
            tokenizers.atxHeading.call(self, eat, rest, true) ||
            tokenizers.setextHeading.call(self, eat, rest, true) ||
            tokenizers.thematicBreak.call(self, eat, rest, true) ||
            tokenizers.html.call(self, eat, rest, true) ||
            tokenizers.list.call(self, eat, rest, true)
          )
        ) ||
        (
          !commonmark &&
          (
            tokenizers.definition.call(self, eat, rest, true) ||
            tokenizers.footnote.call(self, eat, rest, true)
          )
        )
      ) {
        break;
      }
    }

    line = startIndex === index ? content : value.slice(startIndex, nextIndex);

    indents.push(index - startIndex);
    values.push(line);
    contents.push(content);

    index = nextIndex + 1;
  }

  index = -1;
  length = indents.length;
  add = eat(values.join(C_NEWLINE));

  while (++index < length) {
    offsets[currentLine] = (offsets[currentLine] || 0) + indents[index];
    currentLine++;
  }

  exit = self.enterBlock();
  contents = self.tokenizeBlock(contents.join(C_NEWLINE), now);
  exit();

  return add({
    type: 'blockquote',
    children: contents
  });
}
