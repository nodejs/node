"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.drawRow = void 0;
const drawContent_1 = require("./drawContent");
const drawRow = (row, config) => {
    const { border, drawVerticalLine } = config;
    return drawContent_1.drawContent(row, {
        drawSeparator: drawVerticalLine,
        separatorGetter: (index, columnCount) => {
            if (index === 0) {
                return border.bodyLeft;
            }
            if (index === columnCount) {
                return border.bodyRight;
            }
            return border.bodyJoin;
        },
    }) + '\n';
};
exports.drawRow = drawRow;
