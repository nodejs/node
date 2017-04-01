/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module table-pipes
 * @fileoverview
 *   Warn when table rows are not fenced with pipes.
 *
 * @example {"name": "valid.md"}
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   A     | B
 *   ----- | -----
 *   Alpha | Bravo
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1: Missing initial pipe in table fence
 *   1:10: Missing final pipe in table fence
 *   3:1: Missing initial pipe in table fence
 *   3:14: Missing final pipe in table fence
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = tablePipes;

/* Methods. */
var start = position.start;
var end = position.end;

/**
 * Warn when a table rows are not fenced with pipes.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function tablePipes(ast, file) {
  visit(ast, 'table', function (node) {
    var contents = file.toString();

    node.children.forEach(function (row) {
      var cells = row.children;
      var head = cells[0];
      var tail = cells[cells.length - 1];
      var initial = contents.slice(start(row).offset, start(head).offset);
      var final = contents.slice(end(tail).offset, end(row).offset);

      if (position.generated(row)) {
        return;
      }

      if (initial.indexOf('|') === -1) {
        file.message('Missing initial pipe in table fence', start(row));
      }

      if (final.indexOf('|') === -1) {
        file.message('Missing final pipe in table fence', end(row));
      }
    });
  });
}
