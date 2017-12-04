/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module table-cell-padding
 * @fileoverview
 *   Warn when table cells are incorrectly padded.
 *
 *   Options: `'consistent'`, `'padded'`, or `'compact'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used cell padding style and warns when
 *   subsequent cells use different styles.
 *
 * @example {"name": "valid.md", "setting": "padded"}
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example {"name": "valid.md", "setting": "compact"}
 *
 *   |A    |B    |
 *   |-----|-----|
 *   |Alpha|Bravo|
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   |   A    | B    |
 *   |   -----| -----|
 *   |   Alpha| Bravo|
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:5: Cell should be padded with 1 space, not 3
 *   3:10: Cell should be padded
 *   3:17: Cell should be padded
 *
 * @example {"name": "invalid.md", "label": "input", "setting": "padded"}
 *
 *   | A    |    B |
 *   | :----|----: |
 *   | Alpha|Bravo |
 *
 * @example {"name": "invalid.md", "label": "output", "setting": "padded"}
 *
 *   3:8: Cell should be padded
 *   3:9: Cell should be padded
 *
 * @example {"name": "invalid.md", "label": "input", "setting": "compact"}
 *
 *   |A    |     B|
 *   |:----|-----:|
 *   |Alpha|Bravo |
 *
 * @example {"name": "invalid.md", "label": "output", "setting": "compact"}
 *
 *   3:13: Cell should be compact
 *
 * @example {"name": "invalid.md", "label": "output", "setting": "invalid", "config": {"positionless": true}}
 *
 *   1:1: Invalid table-cell-padding style `invalid`
 *
 * @example {"name": "empty-heading.md"}
 *
 *   <!-- Empty heading cells are always OK. -->
 *
 *   |       | Alpha   |
 *   | ----- | ------- |
 *   | Bravo | Charlie |
 *
 * @example {"name": "empty-body.md"}
 *
 *   <!-- Empty body cells are always OK. -->
 *
 *   | Alpha   | Bravo   |
 *   | ------- | ------- |
 *   | Charlie |         |
 */

'use strict';

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:table-cell-padding', tableCellPadding);

var start = position.start;
var end = position.end;

var STYLES = {
  null: true,
  padded: true,
  compact: true
};

function tableCellPadding(tree, file, preferred) {
  preferred = typeof preferred !== 'string' || preferred === 'consistent' ? null : preferred;

  if (STYLES[preferred] !== true) {
    file.fail('Invalid table-cell-padding style `' + preferred + '`');
  }

  visit(tree, 'table', visitor);

  function visitor(node) {
    var rows = node.children;
    var contents = String(file);
    var starts = [];
    var ends = [];
    var cells = [];
    var style;
    var sizes;

    if (generated(node)) {
      return;
    }

    rows.forEach(eachRow);

    sizes = inferSizes(node);

    if (preferred === 'padded') {
      style = 1;
    } else if (preferred === 'compact') {
      style = 0;
    } else {
      style = null;
      starts.concat(ends).some(inferStyle);
    }

    cells.forEach(checkCell);

    function eachRow(row) {
      var children = row.children;

      check(start(row).offset, start(children[0]).offset, null, children[0]);
      ends.pop(); /* Ignore end before row. */

      children.forEach(eachCell);
      starts.pop(); /* Ignore start after row */

      function eachCell(cell, index) {
        var next = children[index + 1] || null;
        check(end(cell).offset, start(next).offset || end(row).offset, cell, next);
        cells.push(cell);
      }
    }

    function inferStyle(pos) {
      if (pos === undefined) {
        return false;
      }

      style = Math.min(pos, 1);
      return true;
    }

    function check(initial, final, prev, next) {
      var fence = contents.slice(initial, final);
      var pos = fence.indexOf('|');

      ends.push(prev && pos !== -1 && prev.children.length !== 0 ? pos : undefined);
      starts.push(next && next.children.length !== 0 ? fence.length - pos - 1 : undefined);
    }

    function checkCell(cell, index) {
      /* Ignore, when compact, every cell except the biggest in the column. */
      if (style === 0 && size(cell) < sizes[index % sizes.length]) {
        return;
      }

      checkSide('start', cell, starts[index], index);
      checkSide('end', cell, ends[index]);
    }

    function checkSide(side, cell, spacing, index) {
      var message;

      if (spacing === undefined || spacing === style) {
        return;
      }

      message = 'Cell should be ';

      if (style === 0) {
        message += 'compact';
      } else {
        message += 'padded';

        if (spacing > style) {
          message += ' with 1 space, not ' + spacing;

          /* May be right or center aligned. */
          if (size(cell) < sizes[index % sizes.length]) {
            return;
          }
        }
      }

      file.message(message, cell.position[side]);
    }
  }
}

function inferSizes(tree) {
  var sizes = Array(tree.align.length);

  tree.children.forEach(row);

  return sizes;

  function row(node) {
    node.children.forEach(cell);
  }

  function cell(node, index) {
    sizes[index] = Math.max(sizes[index] || 0, size(node));
  }
}

function size(node) {
  return end(node).offset - start(node).offset;
}
