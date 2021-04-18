"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _drawHorizontalContent = _interopRequireDefault(require("./drawHorizontalContent"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @typedef {object} drawRow~border
 * @property {string} bodyLeft
 * @property {string} bodyRight
 * @property {string} bodyJoin
 */

/**
 * @param {string[]} columns
 * @param {object} config
 * @param {border} config.border
 * @param {Function} config.drawVerticalLine
 * @returns {string}
 */
const drawRow = (columns, {
  border,
  drawVerticalLine
}) => {
  return (0, _drawHorizontalContent.default)(columns, {
    drawVerticalLine,
    separator: {
      join: border.bodyJoin,
      left: border.bodyLeft,
      right: border.bodyRight
    }
  });
};

var _default = drawRow;
exports.default = _default;