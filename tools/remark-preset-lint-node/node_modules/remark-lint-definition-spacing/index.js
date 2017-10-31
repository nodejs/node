/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module definition-spacing
 * @fileoverview
 *   Warn when consecutive white space is used in a definition.
 *
 * @example {"name": "valid.md"}
 *
 *   [example domain]: http://example.com "Example Domain"
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   [example····domain]: http://example.com "Example Domain"
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:57: Do not use consecutive white-space in definition labels
 */

'use strict';

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:definition-spacing', definitionSpacing);

var LABEL = /^\s*\[((?:\\[\s\S]|[^[\]])+)]/;

function definitionSpacing(tree, file) {
  var contents = file.toString();

  visit(tree, 'definition', validate);
  visit(tree, 'footnoteDefinition', validate);

  function validate(node) {
    var start = position.start(node).offset;
    var end = position.end(node).offset;
    var label;

    if (generated(node)) {
      return;
    }

    label = contents.slice(start, end).match(LABEL)[1];

    if (/[ \t\n]{2,}/.test(label)) {
      file.message('Do not use consecutive white-space in definition labels', node);
    }
  }
}
