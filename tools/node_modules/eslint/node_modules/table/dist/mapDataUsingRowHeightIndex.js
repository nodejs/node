"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _lodash = _interopRequireDefault(require("lodash"));

var _wrapString = _interopRequireDefault(require("./wrapString"));

var _wrapWord = _interopRequireDefault(require("./wrapWord"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {Array} unmappedRows
 * @param {number[]} rowHeightIndex
 * @param {Object} config
 * @returns {Array}
 */
const mapDataUsingRowHeightIndex = (unmappedRows, rowHeightIndex, config) => {
  const tableWidth = unmappedRows[0].length;
  const mappedRows = unmappedRows.map((cells, index0) => {
    const rowHeight = _lodash.default.times(rowHeightIndex[index0], () => {
      return new Array(tableWidth).fill('');
    }); // rowHeight
    //     [{row index within rowSaw; index2}]
    //     [{cell index within a virtual row; index1}]


    cells.forEach((value, index1) => {
      let chunkedValue;

      if (config.columns[index1].wrapWord) {
        chunkedValue = (0, _wrapWord.default)(value, config.columns[index1].width);
      } else {
        chunkedValue = (0, _wrapString.default)(value, config.columns[index1].width);
      }

      chunkedValue.forEach((part, index2) => {
        rowHeight[index2][index1] = part;
      });
    });
    return rowHeight;
  });
  return _lodash.default.flatten(mappedRows);
};

var _default = mapDataUsingRowHeightIndex;
exports.default = _default;
//# sourceMappingURL=mapDataUsingRowHeightIndex.js.map