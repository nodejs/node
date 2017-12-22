'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _stringWidth = require('string-width');

var _stringWidth2 = _interopRequireDefault(_stringWidth);

var _alignString = require('./alignString');

var _alignString2 = _interopRequireDefault(_alignString);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * @param {table~row[]} rows
 * @param {Object} config
 * @returns {table~row[]}
 */
exports.default = (rows, config) => {
  return rows.map(cells => {
    return cells.map((value, index1) => {
      const column = config.columns[index1];

      if ((0, _stringWidth2.default)(value) === column.width) {
        return value;
      } else {
        return (0, _alignString2.default)(value, column.width, column.alignment);
      }
    });
  });
};