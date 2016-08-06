/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module list-item-bullet-indent
 * @fileoverview
 *   Warn when list item bullets are indented.
 *
 * @example {"name": "valid.md"}
 *
 *   Paragraph.
 *
 *   * List item
 *   * List item
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   Paragraph.
 *
 *    * List item
 *    * List item
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:3: Incorrect indentation before bullet: remove 1 space
 *   4:3: Incorrect indentation before bullet: remove 1 space
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var plural = require('plur');

/* Expose. */
module.exports = listItemBulletIndent;

/* Methods. */
var start = position.start;

/**
 * Warn when list item bullets are indented.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function listItemBulletIndent(ast, file) {
  var contents = file.toString();

  visit(ast, 'list', function (node) {
    var items = node.children;

    items.forEach(function (item) {
      var head = item.children[0];
      var initial = start(item).offset;
      var final = start(head).offset;
      var indent;

      if (position.generated(node)) {
        return;
      }

      indent = contents.slice(initial, final).match(/^\s*/)[0].length;

      if (indent !== 0) {
        initial = start(head);

        file.message('Incorrect indentation before bullet: remove ' + indent + ' ' + plural('space', indent), {
          line: initial.line,
          column: initial.column - indent
        });
      }
    });
  });
}
