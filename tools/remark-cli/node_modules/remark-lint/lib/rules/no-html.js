/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-html
 * @fileoverview
 *   Warn when HTML nodes are used.
 *
 *   Ignores comments, because they are used by this tool, remark, and
 *   because markdown doesn’t have native comments.
 *
 * @example {"name": "valid.md"}
 *
 *   # Hello
 *
 *   <!--Comments are also OK-->
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <h1>Hello</h1>
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:15: Do not use HTML in markdown
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = html;

/**
 * Warn when HTML nodes are used.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {*} preferred - Ignored.
 * @param {Function} done - Callback.
 */
function html(ast, file, preferred, done) {
  visit(ast, 'html', function (node) {
    if (!position.generated(node) && !/^\s*<!--/.test(node.value)) {
      file.message('Do not use HTML in markdown', node);
    }
  });

  done();
}
