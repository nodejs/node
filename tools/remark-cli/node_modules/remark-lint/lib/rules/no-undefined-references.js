/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module no-undefined-references
 * @fileoverview
 *   Warn when references to undefined definitions are found.
 *
 * @example {"name": "valid.md"}
 *
 *   [foo][]
 *
 *   [foo]: https://example.com
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   [bar][]
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:8: Found reference to undefined definition
 */

'use strict';

/* Dependencies. */
var position = require('unist-util-position');
var visit = require('unist-util-visit');

/* Expose. */
module.exports = noUnusedDefinitions;

/**
 * Warn when references to undefined definitions are found.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noUnusedDefinitions(ast, file) {
  var map = {};

  visit(ast, 'definition', mark);
  visit(ast, 'footnoteDefinition', mark);

  visit(ast, 'imageReference', find);
  visit(ast, 'linkReference', find);
  visit(ast, 'footnoteReference', find);

  return;

  /**
   * Check `node`.
   *
   * @param {Node} node - Node.
   */
  function mark(node) {
    if (position.generated(node)) {
      return;
    }

    map[node.identifier.toUpperCase()] = true;
  }

  /**
   * Mark `node`.
   *
   * @param {Node} node - Node.
   */
  function find(node) {
    if (position.generated(node)) {
      return;
    }

    if (!map[node.identifier.toUpperCase()]) {
      file.message('Found reference to undefined definition', node);
    }
  }
}
