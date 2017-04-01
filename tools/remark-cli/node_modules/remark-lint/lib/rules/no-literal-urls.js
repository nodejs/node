/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-literal-urls
 * @fileoverview
 *   Warn when URLs without angle-brackets are used.
 *
 * @example {"name": "valid.md"}
 *
 *   <http://foo.bar/baz>
 *   <mailto:qux@quux.com>
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   http://foo.bar/baz
 *
 *   mailto:qux@quux.com
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:19: Don’t use literal URLs without angle brackets
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var toString = require('mdast-util-to-string');
var position = require('unist-util-position');

/* Expose. */
module.exports = noLiteralURLs;

/* Methods. */
var start = position.start;
var end = position.end;

/* Constants. */
var MAILTO = 'mailto:';

/**
 * Warn for literal URLs without angle-brackets.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noLiteralURLs(ast, file) {
  visit(ast, 'link', function (node) {
    var head = start(node.children[0]).column;
    var tail = end(node.children[node.children.length - 1]).column;
    var initial = start(node).column;
    var final = end(node).column;
    var value = toString(node);

    if (position.generated(node)) {
      return;
    }

    if (
      initial === head &&
      final === tail &&
      (node.url === MAILTO + value || node.url === value)
    ) {
      file.message('Don’t use literal URLs without angle brackets', node);
    }
  });
}
