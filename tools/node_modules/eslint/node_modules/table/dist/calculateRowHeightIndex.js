"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _lodash = _interopRequireDefault(require("lodash"));

var _calculateCellHeight = _interopRequireDefault(require("./calculateCellHeight"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Calculates the vertical row span index.
 *
 * @param {Array[]} rows
 * @param {Object} config
 * @returns {number[]}
 */
const calculateRowHeightIndex = (rows, config) => {
  const tableWidth = rows[0].length;
  const rowSpanIndex = [];
  rows.forEach(cells => {
    const cellHeightIndex = new Array(tableWidth).fill(1);
    cells.forEach((value, index1) => {
      if (!_lodash.default.isNumber(config.columns[index1].width)) {
        throw new TypeError('column[index].width must be a number.');
      }

      if (!_lodash.default.isBoolean(config.columns[index1].wrapWord)) {
        throw new TypeError('column[index].wrapWord must be a boolean.');
      }

      cellHeightIndex[index1] = (0, _calculateCellHeight.default)(value, config.columns[index1].width, config.columns[index1].wrapWord);
    });
    rowSpanIndex.push(_lodash.default.max(cellHeightIndex));
  });
  return rowSpanIndex;
};

var _default = calculateRowHeightIndex;
exports.default = _default;
//# sourceMappingURL=calculateRowHeightIndex.js.map