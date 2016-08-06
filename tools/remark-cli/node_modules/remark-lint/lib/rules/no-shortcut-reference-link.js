/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shortcut-reference-link
 * @fileoverview
 *   Warn when shortcut reference links are used.
 *
 * @example {"name": "valid.md"}
 *
 *   [foo][]
 *
 *   [foo]: http://foo.bar/baz
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   [foo]
 *
 *   [foo]: http://foo.bar/baz
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:6: Use the trailing [] on reference links
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = noShortcutReferenceLink;

/**
 * Warn when shortcut reference links are used.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noShortcutReferenceLink(ast, file) {
  visit(ast, 'linkReference', function (node) {
    if (position.generated(node)) {
      return;
    }

    if (node.referenceType === 'shortcut') {
      file.message('Use the trailing [] on reference links', node);
    }
  });
}
