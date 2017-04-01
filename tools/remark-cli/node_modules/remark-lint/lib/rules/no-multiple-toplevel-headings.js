/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-multiple-toplevel-headings
 * @fileoverview
 *   Warn when multiple top-level headings are used.
 *
 *   Options: `number`, default: `1`.
 *
 * @example {"name": "valid.md", "setting": 1}
 *
 *   # Foo
 *
 *   ## Bar
 *
 * @example {"name": "invalid.md", "setting": 1, "label": "input"}
 *
 *   # Foo
 *
 *   # Bar
 *
 * @example {"name": "invalid.md", "setting": 1, "label": "output"}
 *
 *   3:1-3:6: Don’t use multiple top level headings (3:1)
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = noMultipleToplevelHeadings;

/**
 * Warn when multiple top-level headings are used.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {number?} [preferred=1] - Top heading level.
 * @param {Function} done - Callback.
 */
function noMultipleToplevelHeadings(ast, file, preferred) {
  var style = preferred ? preferred : 1;
  var topLevelheading = false;

  visit(ast, 'heading', function (node) {
    var pos;

    if (position.generated(node)) {
      return;
    }

    if (node.depth === style) {
      if (topLevelheading) {
        pos = position.start(node);

        file.message('Don’t use multiple top level headings (' + pos.line + ':' + pos.column + ')', node);
      }

      topLevelheading = node;
    }
  });
}
