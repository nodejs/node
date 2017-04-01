/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module heading-increment
 * @fileoverview
 *   Warn when headings increment with more than 1 level at a time.
 *
 * @example {"name": "valid.md"}
 *
 *   # Alpha
 *
 *   ## Bravo
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   # Charlie
 *
 *   ### Delta
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:1-3:10: Heading levels should increment by one level at a time
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = headingIncrement;

/**
 * Warn when headings increment with more than 1 level at
 * a time.
 *
 * Never warns for the first heading.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function headingIncrement(ast, file) {
  var prev = null;

  visit(ast, 'heading', function (node) {
    var depth = node.depth;

    if (position.generated(node)) {
      return;
    }

    if (prev && depth > prev + 1) {
      file.message('Heading levels should increment by one level at a time', node);
    }

    prev = depth;
  });
}
