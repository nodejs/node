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
 *   <!--Note: `·` represents ` `-->
 *
 *   Lorem ipsum··
 *   dolor sit amet
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <!--Note: `·` represents ` `-->
 *
 *   Lorem ipsum···
 *   dolor sit amet.
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:12-4:1: Use two spaces for hard line breaks
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = hardBreakSpaces;

/**
 * Warn when too many spaces are used to create a
 * hard break.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function hardBreakSpaces(ast, file) {
  var contents = file.toString();

  visit(ast, 'break', function (node) {
    var start = position.start(node).offset;
    var end = position.end(node).offset;
    var value;

    if (position.generated(node)) {
      return;
    }

    value = contents.slice(start, end).split('\n', 1)[0].replace(/\r$/, '');

    if (value.length > 2) {
      file.message('Use two spaces for hard line breaks', node);
    }
  });
}
