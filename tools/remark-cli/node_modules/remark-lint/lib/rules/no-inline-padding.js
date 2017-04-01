/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-inline-padding
 * @fileoverview
 *   Warn when inline nodes are padded with spaces between markers and
 *   content.
 *
 *   Warns for emphasis, strong, delete, image, and link.
 *
 * @example {"name": "valid.md"}
 *
 *   Alpha, *bravo*, _charlie_, [delta](http://echo.fox/trot)
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   Alpha, * bravo *, _ charlie _, [ delta ](http://echo.fox/trot)
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:8-1:17: Don’t pad `emphasis` with inner spaces
 *   1:19-1:30: Don’t pad `emphasis` with inner spaces
 *   1:32-1:63: Don’t pad `link` with inner spaces
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var toString = require('mdast-util-to-string');

/* Expose. */
module.exports = noInlinePadding;

/**
 * Warn when inline nodes are padded with spaces between
 * markers and content.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noInlinePadding(ast, file) {
  visit(ast, function (node) {
    var type = node.type;
    var contents;

    if (position.generated(node)) {
      return;
    }

    if (
      type === 'emphasis' ||
      type === 'strong' ||
      type === 'delete' ||
      type === 'image' ||
      type === 'link'
    ) {
      contents = toString(node);

      if (contents.charAt(0) === ' ' || contents.charAt(contents.length - 1) === ' ') {
        file.message('Don’t pad `' + type + '` with inner spaces', node);
      }
    }
  });
}
