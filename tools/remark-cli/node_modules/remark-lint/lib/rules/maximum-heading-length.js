/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module maximum-heading-length
 * @fileoverview
 *   Warn when headings are too long.
 *
 *   Options: `number`, default: `60`.
 *
 *   Ignores markdown syntax, only checks the plain text content.
 *
 * @example {"name": "valid.md"}
 *
 *   # Alpha bravo charlie delta echo foxtrot golf hotel
 *
 *   # ![Alpha bravo charlie delta echo foxtrot golf hotel](http://example.com/nato.png)
 *
 * @example {"name": "invalid.md", "setting": 40, "label": "input"}
 *
 *   # Alpha bravo charlie delta echo foxtrot golf hotel
 *
 * @example {"name": "invalid.md", "setting": 40, "label": "output"}
 *
 *   1:1-1:52: Use headings shorter than `40`
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var toString = require('mdast-util-to-string');
var position = require('unist-util-position');

/* Expose. */
module.exports = maximumHeadingLength;

/**
 * Warn when headings are too long.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {number?} [preferred=60] - Maximum content
 *   length.
 * @param {Function} done - Callback.
 */
function maximumHeadingLength(ast, file, preferred) {
  preferred = isNaN(preferred) || typeof preferred !== 'number' ? 60 : preferred;

  visit(ast, 'heading', function (node) {
    if (position.generated(node)) {
      return;
    }

    if (toString(node).length > preferred) {
      file.message('Use headings shorter than `' + preferred + '`', node);
    }
  });
}
