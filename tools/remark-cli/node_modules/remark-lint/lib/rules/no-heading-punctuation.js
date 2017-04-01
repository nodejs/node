/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-heading-punctuation
 * @fileoverview
 *   Warn when a heading ends with a a group of characters.
 *   Defaults to `'.,;:!?'`.
 *
 *   Options: `string`, default: `'.,;:!?'`.
 *
 *   Note that these are added to a regex, in a group (`'[' + char + ']'`),
 *   be careful for escapes and dashes.
 *
 * @example {"name": "valid.md"}
 *
 *   # Hello
 *
 * @example {"name": "valid.md", "setting": ",;:!?"}
 *
 *   # Hello...
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   # Hello:
 *
 *   # Hello?
 *
 *   # Hello!
 *
 *   # Hello,
 *
 *   # Hello;
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:9: Don’t add a trailing `:` to headings
 *   3:1-3:9: Don’t add a trailing `?` to headings
 *   5:1-5:9: Don’t add a trailing `!` to headings
 *   7:1-7:9: Don’t add a trailing `,` to headings
 *   9:1-9:9: Don’t add a trailing `;` to headings
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var toString = require('mdast-util-to-string');

/* Expose. */
module.exports = noHeadingPunctuation;

/**
 * Warn when headings end in some characters.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string?} [preferred='\\.,;:!?'] - Group of characters.
 */
function noHeadingPunctuation(ast, file, preferred) {
  preferred = typeof preferred === 'string' ? preferred : '\\.,;:!?';

  visit(ast, 'heading', function (node) {
    var value = toString(node);

    if (position.generated(node)) {
      return;
    }

    value = value.charAt(value.length - 1);

    if (new RegExp('[' + preferred + ']').test(value)) {
      file.message('Don’t add a trailing `' + value + '` to headings', node);
    }
  });
}
