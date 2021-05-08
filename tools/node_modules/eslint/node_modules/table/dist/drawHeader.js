"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.drawHeader = void 0;
const alignString_1 = require("./alignString");
const drawRow_1 = require("./drawRow");
const padTableData_1 = require("./padTableData");
const truncateTableData_1 = require("./truncateTableData");
const wrapCell_1 = require("./wrapCell");
const drawHeader = (width, config) => {
    if (!config.header) {
        throw new Error('Can not draw header without header configuration');
    }
    const { alignment, paddingRight, paddingLeft, wrapWord } = config.header;
    let content = config.header.content;
    content = truncateTableData_1.truncateString(content, config.header.truncate);
    const headerLines = wrapCell_1.wrapCell(content, width, wrapWord);
    return headerLines.map((headerLine) => {
        let line = alignString_1.alignString(headerLine, width, alignment);
        line = padTableData_1.padString(line, paddingLeft, paddingRight);
        return drawRow_1.drawRow([line], {
            ...config,
            drawVerticalLine: (index) => {
                const columnCount = config.columns.length;
                return config.drawVerticalLine(index === 0 ? 0 : columnCount, columnCount);
            },
        });
    }).join('');
};
exports.drawHeader = drawHeader;
