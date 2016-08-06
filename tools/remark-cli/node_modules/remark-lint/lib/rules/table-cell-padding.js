/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module table-cell-padding
 * @fileoverview
 *   Warn when table cells are incorrectly padded.
 *
 *   Options: `string`, either `'consistent'`, `'padded'`, or `'compact'`,
 *   default: `'consistent'`.
 *
 *   The default value, `consistent`, detects the first used cell padding
 *   style, and will warn when a subsequent cells uses a different
 *   style.
 *
 * @example {"name": "valid.md", "setting": "padded"}
 *
 *   <!--Also valid when `consistent`-->
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example {"name": "valid.md", "setting": "compact"}
 *
 *   <!--Also valid when `consistent`-->
 *
 *   |A    |B    |
 *   |-----|-----|
 *   |Alpha|Bravo|
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <!--Always invalid-->
 *
 *   |   A    | B    |
 *   |   -----| -----|
 *   |   Alpha| Bravo|
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:5: Cell should be padded, isn’t
 *   3:9: Cell should be padded, isn’t
 *   3:16: Cell should be padded, isn’t
 *
 * @example {"name": "invalid.md", "label": "output", "setting": "invalid", "config": {"positionless": true}}
 *
 *   1:1: Invalid table-cell-padding style `invalid`
 */

'use strict';

/* eslint-disable max-params */

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = tableCellPadding;

/* Methods. */
var start = position.start;
var end = position.end;

/* Valid styles. */
var STYLES = {
  null: true,
  padded: true,
  compact: true
};

/**
 * Warn when table cells are incorrectly padded.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string?} preferred - Either `padded` (for
 *   at least a space), `compact` (for no spaces when
 *   possible), or `consistent`, which defaults to the
 *   first found style.
 */
function tableCellPadding(ast, file, preferred) {
  preferred = typeof preferred !== 'string' || preferred === 'consistent' ? null : preferred;

  if (STYLES[preferred] !== true) {
    file.fail('Invalid table-cell-padding style `' + preferred + '`');
  }

  visit(ast, 'table', function (node) {
    var children = node.children;
    var contents = file.toString();
    var starts = [];
    var ends = [];
    var locations;
    var positions;
    var style;
    var type;
    var warning;

    if (position.generated(node)) {
      return;
    }

    /**
     * Check a fence. Checks both its initial spacing
     * (between a cell and the fence), and its final
     * spacing (between the fence and the next cell).
     *
     * @param {number} initial - Starting index.
     * @param {number} final - Closing index.
     * @param {Node} cell - Table cell.
     * @param {Node?} next - Following cell.
     * @param {number} index - Position of `cell` in
     *   its parent.
     */
    function check(initial, final, cell, next, index) {
      var fence = contents.slice(initial, final);
      var pos = fence.indexOf('|');

      if (
        cell &&
        pos !== -1 &&
        (ends[index] === undefined || pos < ends[index])
      ) {
        ends[index] = pos;
      }

      if (next && pos !== -1) {
        pos = fence.length - pos - 1;

        if (starts[index + 1] === undefined || pos < starts[index + 1]) {
          starts[index + 1] = pos;
        }
      }
    }

    children.forEach(function (row) {
      var cells = row.children;

      check(start(row).offset, start(cells[0]).offset, null, cells[0], -1);

      cells.forEach(function (cell, index) {
        var next = cells[index + 1] || null;
        var final = start(next).offset || end(row).offset;

        check(end(cell).offset, final, cell, next, index);
      });
    });

    positions = starts.concat(ends);

    if (preferred === 'padded') {
      style = 1;
    } else if (preferred === 'compact') {
      style = 0;
    } else {
      style = null;
    }

    if (preferred === 'padded') {
      style = 1;
    } else if (preferred === 'compact') {
      style = 0;
    } else {
      positions.some(function (pos) {
        /* `some` skips non-existant indices, so
         * there's no need to check for `!isNaN`. */
        style = Math.min(pos, 1);

        return true;
      });
    }

    locations = children[0].children.map(function (cell) {
      return start(cell);
    }).concat(children[0].children.map(function (cell) {
      return end(cell);
    }));

    type = style === 1 ? 'padded' : 'compact';
    warning = 'Cell should be ' + type + ', isn’t';

    positions.forEach(function (diff, index) {
      if (diff !== style && diff !== undefined && diff !== null) {
        file.message(warning, locations[index]);
      }
    });
  });
}
