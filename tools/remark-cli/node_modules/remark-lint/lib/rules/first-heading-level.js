/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module first-heading-level
 * @fileoverview
 *   Warn when the first heading has a level other than a specified value.
 *
 *   Options: `number`, default: `1`.
 *
 * @example {"name": "valid.md", "setting": 1}
 *
 *   <!-- Also valid by default. -->
 *
 *   # Alpha
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "setting": 1, "label": "input"}
 *
 *   <!-- Also invalid by default. -->
 *
 *   ## Bravo
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "setting": 1, "label": "output"}
 *
 *   3:1-3:9: First heading level should be `1`
 *
 * @example {"name": "valid.md", "setting": 2}
 *
 *   ## Bravo
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "setting": 2, "label": "input"}
 *
 *   # Bravo
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "setting": 2, "label": "output"}
 *
 *   1:1-1:8: First heading level should be `2`
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = firstHeadingLevel;

/**
 * Warn when the first heading has a level other than a specified value.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {number?} [preferred=1] - First heading level.
 */
function firstHeadingLevel(ast, file, preferred) {
  var style = preferred && preferred !== true ? preferred : 1;

  visit(ast, 'heading', function (node) {
    if (position.generated(node)) {
      return;
    }

    if (node.depth !== style) {
      file.message('First heading level should be `' + style + '`', node);
    }

    return false;
  });
}
