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

var rule = require('unified-lint-rule');
var generated = require('unist-util-generated');
var visit = require('unist-util-visit');

module.exports = rule('remark-lint:no-unused-definitions', noUnusedDefinitions);

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

  function find(node) {
    if (generated(node)) {
      return;
    }

    map[node.identifier.toUpperCase()] = {
      node: node,
      used: false
    };
  }

  function mark(node) {
    var info = map[node.identifier.toUpperCase()];

    if (generated(node) || !info) {
      return;
    }

    info.used = true;
  }
}
