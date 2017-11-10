/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-table-indentation
 * @fileoverview
 *   Warn when tables are indented.
 *
 * @example {"name": "valid.md"}
 *
 *   Paragraph.
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   Paragraph.
 *
 *   ···| A     | B     |
 *   ···| ----- | ----- |
 *   ···| Alpha | Bravo |
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:1-3:21: Do not indent table rows
 *   5:1-5:21: Do not indent table rows
 */

'use strict';

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:no-table-indentation', noTableIndentation);

function noTableIndentation(ast, file) {
  visit(ast, 'table', visitor);

  function visitor(node) {
    var contents = file.toString();

    if (generated(node)) {
      return;
    }

    node.children.forEach(each);

    function each(row) {
      var fence = contents.slice(position.start(row).offset, position.start(row.children[0]).offset);

      if (fence.indexOf('|') > 1) {
        file.message('Do not indent table rows', row);
      }
    }
  }
}
