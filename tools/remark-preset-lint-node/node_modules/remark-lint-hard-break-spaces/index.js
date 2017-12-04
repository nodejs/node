/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module hard-break-spaces
 * @fileoverview
 *   Warn when too many spaces are used to create a hard break.
 *
 * @example {"name": "valid.md"}
 *
 *   Lorem ipsum··
 *   dolor sit amet
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   Lorem ipsum···
 *   dolor sit amet.
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:12-2:1: Use two spaces for hard line breaks
 */

'use strict';

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:hard-break-spaces', hardBreakSpaces);

function hardBreakSpaces(ast, file) {
  var contents = file.toString();

  visit(ast, 'break', visitor);

  function visitor(node) {
    var start = position.start(node).offset;
    var end = position.end(node).offset;
    var value;

    if (generated(node)) {
      return;
    }

    value = contents.slice(start, end).split('\n', 1)[0].replace(/\r$/, '');

    if (value.length > 2) {
      file.message('Use two spaces for hard line breaks', node);
    }
  }
}
