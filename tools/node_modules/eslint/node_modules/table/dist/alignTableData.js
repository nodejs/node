"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.alignTableData = void 0;
const alignString_1 = require("./alignString");
const alignTableData = (rows, config) => {
    return rows.map((row) => {
        return row.map((cell, cellIndex) => {
            const { width, alignment } = config.columns[cellIndex];
            return alignString_1.alignString(cell, width, alignment);
        });
    });
};
exports.alignTableData = alignTableData;
