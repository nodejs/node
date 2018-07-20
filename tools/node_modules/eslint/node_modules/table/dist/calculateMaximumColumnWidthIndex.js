'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _calculateCellWidthIndex = require('./calculateCellWidthIndex');

var _calculateCellWidthIndex2 = _interopRequireDefault(_calculateCellWidthIndex);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Produces an array of values that describe the largest value length (width) in every column.
 *
 * @param {Array[]} rows
 * @returns {number[]}
 */
exports.default = rows => {
  if (!rows[0]) {
    throw new Error('Dataset must have at least one row.');
  }

  const columns = Array(rows[0].length).fill(0);

  rows.forEach(row => {
    const columnWidthIndex = (0, _calculateCellWidthIndex2.default)(row);

    columnWidthIndex.forEach((valueWidth, index0) => {
      if (columns[index0] < valueWidth) {
        columns[index0] = valueWidth;
      }
    });
  });

  return columns;
};