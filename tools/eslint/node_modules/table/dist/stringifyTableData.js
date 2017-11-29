"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

/**
 * Casts all cell values to a string.
 *
 * @param {table~row[]} rows
 * @returns {table~row[]}
 */
exports.default = rows => {
  return rows.map(cells => {
    return cells.map(String);
  });
};