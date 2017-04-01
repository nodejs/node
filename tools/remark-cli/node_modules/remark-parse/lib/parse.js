/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:parse
 * @fileoverview Parse the document
 */

'use strict';

/* Dependencies. */
var xtend = require('xtend');
var removePosition = require('unist-util-remove-position');

/* Expose. */
module.exports = parse;

/* Characters. */
var C_NEWLINE = '\n';

/* Constants. */
var EXPRESSION_LINE_BREAKS = /\r\n|\r/g;

/**
 * Parse the bound file.
 *
 * @example
 *   new Parser(new File('_Foo_.')).parse();
 *
 * @this {Parser}
 * @return {Object} - `root` node.
 */
function parse() {
  var self = this;
  var value = String(self.file);
  var start = {line: 1, column: 1, offset: 0};
  var content = xtend(start);
  var node;

  /* Clean non-unix newlines: `\r\n` and `\r` are all
   * changed to `\n`.  This should not affect positional
   * information. */
  value = value.replace(EXPRESSION_LINE_BREAKS, C_NEWLINE);

  if (value.charCodeAt(0) === 0xFEFF) {
    value = value.slice(1);

    content.column++;
    content.offset++;
  }

  node = {
    type: 'root',
    children: self.tokenizeBlock(value, content),
    position: {
      start: start,
      end: self.eof || xtend(start)
    }
  };

  if (!self.options.position) {
    removePosition(node, true);
  }

  return node;
}
