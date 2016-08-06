/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module definition-case
 * @fileoverview
 *   Warn when definition labels are not lower-case.
 *
 * @example {"name": "valid.md"}
 *
 *   [example]: http://example.com "Example Domain"
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   [Example]: http://example.com "Example Domain"
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:47: Do not use upper-case characters in definition labels
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = definitionCase;

/* Expressions. */
var LABEL = /^\s*\[((?:\\[\s\S]|[^\[\]])+)\]/;

/**
 * Warn when definitions are not placed at the end of the
 * file.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function definitionCase(ast, file) {
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

    if (label !== label.toLowerCase()) {
      file.message('Do not use upper-case characters in definition labels', node);
    }
  }
}
