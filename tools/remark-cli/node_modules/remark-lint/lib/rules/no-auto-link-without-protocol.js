/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-auto-link-without-protocol
 * @fileoverview
 *   Warn for angle-bracketed links without protocol.
 *
 * @example {"name": "valid.md"}
 *
 *   <http://www.example.com>
 *   <mailto:foo@bar.com>
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <www.example.com>
 *   <foo@bar.com>
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   2:1-2:14: All automatic links must start with a protocol
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var visit = require('unist-util-visit');
var toString = require('mdast-util-to-string');
var position = require('unist-util-position');

/* Expose. */
module.exports = noAutoLinkWithoutProtocol;

/* Methods. */
var start = position.start;
var end = position.end;

/**
 * Protocol expression.
 *
 * @type {RegExp}
 * @see http://en.wikipedia.org/wiki/URI_scheme#Generic_syntax
 */

var PROTOCOL = /^[a-z][a-z+.-]+:\/?/i;

/**
 * Warn for angle-bracketed links without protocol.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noAutoLinkWithoutProtocol(ast, file) {
  visit(ast, 'link', function (node) {
    var head = start(node.children[0]).column;
    var tail = end(node.children[node.children.length - 1]).column;
    var initial = start(node).column;
    var final = end(node).column;

    if (position.generated(node)) {
      return;
    }

    if (initial === head - 1 && final === tail + 1 && !hasProtocol(node)) {
      file.message('All automatic links must start with a protocol', node);
    }
  });
}

/**
 * Assert `node`s reference starts with a protocol.
 *
 * @param {Node} node - Node to test.
 * @return {boolean} - Whether `node` has a protocol.
 */
function hasProtocol(node) {
  return PROTOCOL.test(toString(node));
}
