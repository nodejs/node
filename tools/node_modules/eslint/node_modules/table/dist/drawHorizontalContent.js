"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

/**
 * @typedef {object} drawHorizontalContent~separator
 * @property {string} left
 * @property {string} right
 * @property {string} join
 */

/**
 * @param {string[]} columns
 * @param {object} config
 * @param {drawHorizontalContent~separator} config.separator
 * @param {Function} config.drawVerticalLine
 * @returns {string}
 */
const drawHorizontalContent = (columns, {
  separator,
  drawVerticalLine
}) => {
  const columnCount = columns.length;
  const result = [];
  result.push(drawVerticalLine(0, columnCount) ? separator.left : '');
  columns.forEach((column, index) => {
    result.push(column);

    if (index + 1 < columnCount) {
      result.push(drawVerticalLine(index + 1, columnCount) ? separator.join : '');
    }
  });
  result.push(drawVerticalLine(columnCount, columnCount) ? separator.right : '');
  return result.join('') + '\n';
};

var _default = drawHorizontalContent;
exports.default = _default;