/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module no-unused-definitions
 * @fileoverview
 *   Warn when unused definitions are found.
 *
 * @example {"name": "valid.md"}
 *
 *   [foo][]
 *
 *   [foo]: https://example.com
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   [bar]: https://example.com
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:27: Found unused definition
 */

'use strict';

/* Dependencies. */
var position = require('unist-util-position');
var visit = require('unist-util-visit');

/* Expose. */
module.exports = noUnusedDefinitions;

/**
 * Warn when unused definitions are found.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noUnusedDefinitions(ast, file) {
  var map = {};
  var identifier;

  visit(ast, 'definition', find);
  visit(ast, 'footnoteDefinition', find);

  visit(ast, 'imageReference', mark);
  visit(ast, 'linkReference', mark);
  visit(ast, 'footnoteReference', mark);

  for (identifier in map) {
    if (!map[identifier].used) {
      file.message('Found unused definition', map[identifier].node);
    }
  }

  return;

  /**
   * Check `node`.
   *
   * @param {Node} node - Node.
   */
  function find(node) {
    if (position.generated(node)) {
      return;
    }

    map[node.identifier.toUpperCase()] = {
      node: node,
      used: false
    };
  }

  /**
   * Mark `node`.
   *
   * @param {Node} node - Node.
   */
  function mark(node) {
    var info = map[node.identifier.toUpperCase()];

    if (position.generated(node) || !info) {
      return;
    }

    info.used = true;
  }
}
