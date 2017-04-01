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
 *   [example    domain]: http://example.com "Example Domain"
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:57: Do not use consecutive white-space in definition labels
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = definitionSpacing;

/* Expressions. */
var LABEL = /^\s*\[((?:\\[\s\S]|[^\[\]])+)\]/;

/**
 * Warn when consecutive white space is used in a
 * definition.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function definitionSpacing(ast, file) {
  var contents = file.toString();

  visit(ast, 'definition', validate);
  visit(ast, 'footnoteDefinition', validate);

  return;

  /**
   * Validate a node, either a normal definition or
   * a footnote definition.
   *
   * @param {Node} node - Node.
   */
  function validate(node) {
    var start = position.start(node).offset;
    var end = position.end(node).offset;
    var label;

    if (position.generated(node)) {
      return;
    }

    label = contents.slice(start, end).match(LABEL)[1];

    if (/[ \t\n]{2,}/.test(label)) {
      file.message('Do not use consecutive white-space in definition labels', node);
    }
  }
}
