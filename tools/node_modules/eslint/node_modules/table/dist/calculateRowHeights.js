"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.calculateRowHeights = void 0;
const calculateCellHeight_1 = require("./calculateCellHeight");
/**
 * Produces an array of values that describe the largest value length (height) in every row.
 */
const calculateRowHeights = (rows, config) => {
    return rows.map((row) => {
        let rowHeight = 1;
        row.forEach((cell, cellIndex) => {
            const cellHeight = calculateCellHeight_1.calculateCellHeight(cell, config.columns[cellIndex].width, config.columns[cellIndex].wrapWord);
            rowHeight = Math.max(rowHeight, cellHeight);
        });
        return rowHeight;
    });
};
exports.calculateRowHeights = calculateRowHeights;
