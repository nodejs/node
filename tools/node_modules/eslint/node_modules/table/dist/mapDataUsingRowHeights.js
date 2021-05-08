"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.mapDataUsingRowHeights = void 0;
const wrapCell_1 = require("./wrapCell");
const createEmptyStrings = (length) => {
    return new Array(length).fill('');
};
const padCellVertically = (lines, rowHeight, columnConfig) => {
    const { verticalAlignment } = columnConfig;
    const availableLines = rowHeight - lines.length;
    if (verticalAlignment === 'top') {
        return [...lines, ...createEmptyStrings(availableLines)];
    }
    if (verticalAlignment === 'bottom') {
        return [...createEmptyStrings(availableLines), ...lines];
    }
    return [
        ...createEmptyStrings(Math.floor(availableLines / 2)),
        ...lines,
        ...createEmptyStrings(Math.ceil(availableLines / 2)),
    ];
};
const flatten = (array) => {
    return [].concat(...array);
};
const mapDataUsingRowHeights = (unmappedRows, rowHeights, config) => {
    const tableWidth = unmappedRows[0].length;
    const mappedRows = unmappedRows.map((unmappedRow, unmappedRowIndex) => {
        const outputRowHeight = rowHeights[unmappedRowIndex];
        const outputRow = Array.from({ length: outputRowHeight }, () => {
            return new Array(tableWidth).fill('');
        });
        unmappedRow.forEach((cell, cellIndex) => {
            const cellLines = wrapCell_1.wrapCell(cell, config.columns[cellIndex].width, config.columns[cellIndex].wrapWord);
            const paddedCellLines = padCellVertically(cellLines, outputRowHeight, config.columns[cellIndex]);
            paddedCellLines.forEach((cellLine, cellLineIndex) => {
                outputRow[cellLineIndex][cellIndex] = cellLine;
            });
        });
        return outputRow;
    });
    return flatten(mappedRows);
};
exports.mapDataUsingRowHeights = mapDataUsingRowHeights;
