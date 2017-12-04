/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-inline-padding
 * @fileoverview
 *   Warn when inline nodes are padded with spaces between their markers and
 *   content.
 *
 *   Warns for emphasis, strong, delete, images, and links.
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

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var generated = require('unist-util-generated');
var toString = require('mdast-util-to-string');

module.exports = rule('remark-lint:no-inline-padding', noInlinePadding);

function noInlinePadding(ast, file) {
  visit(ast, visitor);

  function visitor(node) {
    var type = node.type;
    var contents;

    if (generated(node)) {
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
  }
}
