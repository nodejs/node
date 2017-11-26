/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shortcut-reference-image
 * @fileoverview
 *   Warn when shortcut reference images are used.
 *
 *   Shortcut references render as images when a definition is found, and as
 *   plain text without definition.  Sometimes, you don’t intend to create an
 *   image from the reference, but this rule still warns anyway.  In that case,
 *   you can escape the reference like so: `!\[foo]`.
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

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:no-shortcut-reference-image', noShortcutReferenceImage);

function noShortcutReferenceImage(ast, file) {
  visit(ast, 'imageReference', visitor);

  function visitor(node) {
    if (generated(node)) {
      return;
    }

    if (node.referenceType === 'shortcut') {
      file.message('Use the trailing [] on reference images', node);
    }
  }
}
