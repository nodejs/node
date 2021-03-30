"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _lodash = _interopRequireDefault(require("lodash.flatten"));

var _wrapCell = _interopRequireDefault(require("./wrapCell"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {Array} unmappedRows
 * @param {number[]} rowHeightIndex
 * @param {object} config
 * @returns {Array}
 */
const mapDataUsingRowHeightIndex = (unmappedRows, rowHeightIndex, config) => {
  const tableWidth = unmappedRows[0].length;
  const mappedRows = unmappedRows.map((cells, index0) => {
    const rowHeight = Array.from(new Array(rowHeightIndex[index0]), () => {
      return new Array(tableWidth).fill('');
    }); // rowHeight
    //     [{row index within rowSaw; index2}]
    //     [{cell index within a virtual row; index1}]

    cells.forEach((value, index1) => {
      const cellLines = (0, _wrapCell.default)(value, config.columns[index1].width, config.columns[index1].wrapWord);
      cellLines.forEach((cellLine, index2) => {
        rowHeight[index2][index1] = cellLine;
      });
    });
    return rowHeight;
  });
  return (0, _lodash.default)(mappedRows);
};

var _default = mapDataUsingRowHeightIndex;
exports.default = _default;