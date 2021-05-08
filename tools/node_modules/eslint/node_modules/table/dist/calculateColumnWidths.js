"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const calculateCellWidths_1 = require("./calculateCellWidths");
/**
 * Produces an array of values that describe the largest value length (width) in every column.
 */
exports.default = (rows) => {
    const columnWidths = new Array(rows[0].length).fill(0);
    rows.forEach((row) => {
        const cellWidths = calculateCellWidths_1.calculateCellWidths(row);
        cellWidths.forEach((cellWidth, cellIndex) => {
            columnWidths[cellIndex] = Math.max(columnWidths[cellIndex], cellWidth);
        });
    });
    return columnWidths;
};
