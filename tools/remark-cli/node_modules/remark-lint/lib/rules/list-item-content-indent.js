/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module list-item-content-indent
 * @fileoverview
 *   Warn when the content of a list item has mixed indentation.
 *
 * @example {"name": "valid.md"}
 *
 *   1. [x] Alpha
 *      1. Bravo
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   1. [x] Charlie
 *       1. Delta
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   2:5: Don’t use mixed indentation for children, remove 1 space
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var plural = require('plur');

/* Expose. */
module.exports = listItemContentIndent;

/* Methods. */
var start = position.start;

/**
 * Warn when the content of a list item has mixed
 * indentation.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function listItemContentIndent(ast, file) {
  var contents = file.toString();

  visit(ast, 'listItem', function (node) {
    var style;

    node.children.forEach(function (item, index) {
      var begin = start(item);
      var column = begin.column;
      var char;
      var diff;
      var word;

      if (position.generated(item)) {
        return;
      }

      /* Get indentation for the first child.
       * Only the first item can have a checkbox,
       * so here we remove that from the column. */
      if (index === 0) {
        /* If there’s a checkbox before the content,
         * look backwards to find the start of that
         * checkbox. */
        if (Boolean(node.checked) === node.checked) {
          char = begin.offset - 1;

          while (contents.charAt(char) !== '[') {
            char--;
          }

          column -= begin.offset - char;
        }

        style = column;

        return;
      }

      /* Warn for violating children. */
      if (column !== style) {
        diff = style - column;
        word = diff > 0 ? 'add' : 'remove';

        diff = Math.abs(diff);

        file.message(
          'Don’t use mixed indentation for children, ' + word +
          ' ' + diff + ' ' + plural('space', diff),
          {
            line: start(item).line,
            column: column
          }
        );
      }
    });
  });
}
