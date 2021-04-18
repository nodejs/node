"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _drawBorder = require("./drawBorder");

var _drawRow = _interopRequireDefault(require("./drawRow"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {string[][]} rows
 * @param {Array} columnSizeIndex
 * @param {Array} rowSpanIndex
 * @param {table~config} config
 * @returns {string}
 */
const drawTable = (rows, columnSizeIndex, rowSpanIndex, config) => {
  const {
    drawHorizontalLine,
    singleLine
  } = config;
  let output;
  let realRowIndex;
  let rowHeight;
  const rowCount = rows.length;
  realRowIndex = 0;
  output = '';

  if (drawHorizontalLine(realRowIndex, rowCount)) {
    output += (0, _drawBorder.drawBorderTop)(columnSizeIndex, config);
  }

  rows.forEach((row, index0) => {
    output += (0, _drawRow.default)(row, config);

    if (!rowHeight) {
      rowHeight = rowSpanIndex[realRowIndex];
      realRowIndex++;
    }

    rowHeight--;

    if (!singleLine && rowHeight === 0 && index0 !== rowCount - 1 && drawHorizontalLine(realRowIndex, rowCount)) {
      output += (0, _drawBorder.drawBorderJoin)(columnSizeIndex, config);
    }
  });

  if (drawHorizontalLine(realRowIndex, rowCount)) {
    output += (0, _drawBorder.drawBorderBottom)(columnSizeIndex, config);
  }

  return output;
};

var _default = drawTable;
exports.default = _default;