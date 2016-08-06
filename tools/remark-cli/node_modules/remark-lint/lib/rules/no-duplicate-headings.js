/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-duplicate-headings
 * @fileoverview
 *   Warn when duplicate headings are found.
 *
 * @example {"name": "valid.md"}
 *
 *   # Foo
 *
 *   ## Bar
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   # Foo
 *
 *   ## Foo
 *
 *   ## [Foo](http://foo.com/bar)
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:1-3:7: Do not use headings with similar content (1:1)
 *   5:1-5:29: Do not use headings with similar content (3:1)
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var position = require('unist-util-position');
var visit = require('unist-util-visit');
var toString = require('mdast-util-to-string');

/* Expose. */
module.exports = noDuplicateHeadings;

/**
 * Warn when headings with equal content are found.
 *
 * Matches case-insensitive.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noDuplicateHeadings(ast, file) {
  var map = {};

  visit(ast, 'heading', function (node) {
    var value = toString(node).toUpperCase();
    var duplicate = map[value];
    var pos;

    if (position.generated(node)) {
      return;
    }

    if (duplicate && duplicate.type === 'heading') {
      pos = position.start(duplicate);

      file.message(
        'Do not use headings with similar content (' +
        pos.line + ':' + pos.column + ')',
        node
      );
    }

    map[value] = node;
  });
}
