/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module final-definition
 * @fileoverview
 *   Warn when definitions are not placed at the end of the file.
 *
 * @example {"name": "valid.md"}
 *
 *   Paragraph.
 *
 *   [example]: http://example.com "Example Domain"
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   Paragraph.
 *
 *   [example]: http://example.com "Example Domain"
 *
 *   Another paragraph.
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:1-3:47: Move definitions to the end of the file (after the node at line `5`)
 */

'use strict';

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:final-definition', finalDefinition);

var start = position.start;

function finalDefinition(ast, file) {
  var last = null;

  visit(ast, visitor, true);

  function visitor(node) {
    var line = start(node).line;

    /* Ignore generated nodes. */
    if (node.type === 'root' || generated(node)) {
      return;
    }

    if (node.type === 'definition') {
      if (last !== null && last > line) {
        file.message('Move definitions to the end of the file (after the node at line `' + last + '`)', node);
      }
    } else if (last === null) {
      last = line;
    }
  }
}
