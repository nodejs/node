"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _lodash = _interopRequireDefault(require("lodash"));

var _stringWidth = _interopRequireDefault(require("string-width"));

var _wrapWord = _interopRequireDefault(require("./wrapWord"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {string} value
 * @param {number} columnWidth
 * @param {boolean} useWrapWord
 * @returns {number}
 */
const calculateCellHeight = (value, columnWidth, useWrapWord = false) => {
  if (!_lodash.default.isString(value)) {
    throw new TypeError('Value must be a string.');
  }

  if (!Number.isInteger(columnWidth)) {
    throw new TypeError('Column width must be an integer.');
  }

  if (columnWidth < 1) {
    throw new Error('Column width must be greater than 0.');
  }

  if (useWrapWord) {
    return (0, _wrapWord.default)(value, columnWidth).length;
  }

  return Math.ceil((0, _stringWidth.default)(value) / columnWidth);
};

var _default = calculateCellHeight;
exports.default = _default;
//# sourceMappingURL=calculateCellHeight.js.map