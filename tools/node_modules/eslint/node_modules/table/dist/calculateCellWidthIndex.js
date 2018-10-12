"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _stringWidth = _interopRequireDefault(require("string-width"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Calculates width of each cell contents.
 *
 * @param {string[]} cells
 * @returns {number[]}
 */
const calculateCellWidthIndex = cells => {
  return cells.map(value => {
    return (0, _stringWidth.default)(value);
  });
};

var _default = calculateCellWidthIndex;
exports.default = _default;
//# sourceMappingURL=calculateCellWidthIndex.js.map