/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shortcut-reference-image
 * @fileoverview
 *   Warn when shortcut reference images are used.
 *
 * @example {"name": "valid.md"}
 *
 *   ![foo][]
 *
 *   [foo]: http://foo.bar/baz.png
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   ![foo]
 *
 *   [foo]: http://foo.bar/baz.png
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:7: Use the trailing [] on reference images
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = noShortcutReferenceImage;

/**
 * Warn when shortcut reference images are used.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noShortcutReferenceImage(ast, file) {
  visit(ast, 'imageReference', function (node) {
    if (position.generated(node)) {
      return;
    }

    if (node.referenceType === 'shortcut') {
      file.message('Use the trailing [] on reference images', node);
    }
  });
}
