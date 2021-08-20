"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.padTableData = exports.padString = void 0;
const padString = (input, paddingLeft, paddingRight) => {
    return ' '.repeat(paddingLeft) + input + ' '.repeat(paddingRight);
};
exports.padString = padString;
const padTableData = (rows, config) => {
    return rows.map((cells) => {
        return cells.map((cell, cellIndex) => {
            const { paddingLeft, paddingRight } = config.columns[cellIndex];
            return exports.padString(cell, paddingLeft, paddingRight);
        });
    });
};
exports.padTableData = padTableData;
