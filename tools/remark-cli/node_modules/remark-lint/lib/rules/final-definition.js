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

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = finalDefinition;

/* Methods. */
var start = position.start;

/**
 * Warn when definitions are not placed at the end of
 * the file.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function finalDefinition(ast, file) {
  var last = null;

  visit(ast, function (node) {
    var line = start(node).line;

    /* Ignore generated nodes. */
    if (node.type === 'root' || position.generated(node)) {
      return;
    }

    if (node.type === 'definition') {
      if (last !== null && last > line) {
        file.message('Move definitions to the end of the file (after the node at line `' + last + '`)', node);
      }
    } else if (last === null) {
      last = line;
    }
  }, true);
}
