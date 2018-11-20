'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _stringWidth = require('string-width');

var _stringWidth2 = _interopRequireDefault(_stringWidth);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Calculates width of each cell contents.
 *
 * @param {string[]} cells
 * @returns {number[]}
 */
exports.default = cells => {
  return cells.map(value => {
    return (0, _stringWidth2.default)(value);
  });
};