'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});

/**
 * @param {table~row[]} rows
 * @param {Object} config
 * @returns {table~row[]}
 */
exports.default = (rows, config) => {
  return rows.map(cells => {
    return cells.map((value, index1) => {
      const column = config.columns[index1];

      return ' '.repeat(column.paddingLeft) + value + ' '.repeat(column.paddingRight);
    });
  });
};