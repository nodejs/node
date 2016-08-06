/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module list-item-spacing
 * @fileoverview
 *   Warn when list looseness is incorrect, such as being tight
 *   when it should be loose, and vice versa.
 *
 *   According to the [markdown-style-guide](http://www.cirosantilli.com/markdown-style-guide/),
 *   if one or more list-items in a list spans more than one line,
 *   the list is required to have blank lines between each item.
 *   And otherwise, there should not be blank lines between items.
 *
 * @example {"name": "valid.md"}
 *
 *   A tight list:
 *
 *   -   item 1
 *   -   item 2
 *   -   item 3
 *
 *   A loose list:
 *
 *   -   Wrapped
 *       item
 *
 *   -   item 2
 *
 *   -   item 3
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   A tight list:
 *
 *   -   Wrapped
 *       item
 *   -   item 2
 *   -   item 3
 *
 *   A loose list:
 *
 *   -   item 1
 *
 *   -   item 2
 *
 *   -   item 3
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   4:9-5:1: Missing new line after list item
 *   5:11-6:1: Missing new line after list item
 *   11:1-12:1: Extraneous new line after list item
 *   13:1-14:1: Extraneous new line after list item
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = listItemSpacing;

/* Methods. */
var start = position.start;
var end = position.end;

/**
 * Warn when list items looseness is incorrect, such as
 * being tight when it should be loose, and vice versa.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function listItemSpacing(ast, file) {
  visit(ast, 'list', function (node) {
    var items = node.children;
    var isTightList = true;
    var indent = start(node).column;
    var type;

    if (position.generated(node)) {
      return;
    }

    items.forEach(function (item) {
      var content = item.children;
      var head = content[0];
      var tail = content[content.length - 1];
      var isLoose = (end(tail).line - start(head).line) > 0;

      if (isLoose) {
        isTightList = false;
      }
    });

    type = isTightList ? 'tight' : 'loose';

    items.forEach(function (item, index) {
      var next = items[index + 1];
      var isTight = end(item).column > indent;

      /* Ignore last. */
      if (!next) {
        return;
      }

      /* Check if the list item's state does (not)
       * match the list's state. */
      if (isTight !== isTightList) {
        if (type === 'loose') {
          file.message('Missing new line after list item', {
            start: end(item),
            end: start(next)
          });
        } else {
          file.message('Extraneous new line after list item', {
            start: end(item),
            end: start(next)
          });
        }
      }
    });
  });
}
