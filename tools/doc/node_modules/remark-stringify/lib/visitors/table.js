/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:table
 * @fileoverview Stringify a table.
 */

'use strict';

/* Dependencies. */
var markdownTable = require('markdown-table');

/* Expose. */
module.exports = table;

/**
 * Stringify table.
 *
 * Creates a fenced table by default, but not in
 * `looseTable: true` mode:
 *
 *     Foo | Bar
 *     :-: | ---
 *     Baz | Qux
 *
 * NOTE: Be careful with `looseTable: true` mode, as a
 * loose table inside an indented code block on GitHub
 * renders as an actual table!
 *
 * Creates a spaced table by default, but not in
 * `spacedTable: false`:
 *
 *     |Foo|Bar|
 *     |:-:|---|
 *     |Baz|Qux|
 *
 * @param {Object} node - `table` node.
 * @return {string} - Markdown table.
 */
function table(node) {
  var self = this;
  var loose = self.options.looseTable;
  var spaced = self.options.spacedTable;
  var pad = self.options.paddedTable;
  var rows = node.children;
  var index = rows.length;
  var exit = self.enterTable();
  var result = [];
  var start;
  var end;

  while (index--) {
    result[index] = self.all(rows[index]);
  }

  exit();

  if (loose) {
    start = '';
    end = '';
  } else if (spaced) {
    start = '| ';
    end = ' |';
  } else {
    start = '|';
    end = '|';
  }

  return markdownTable(result, {
    align: node.align,
    pad: pad,
    start: start,
    end: end,
    delimiter: spaced ? ' | ' : '|'
  });
}
