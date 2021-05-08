"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.drawBorderTop = exports.drawBorderJoin = exports.drawBorderBottom = exports.drawBorder = exports.createTableBorderGetter = void 0;
const drawContent_1 = require("./drawContent");
const drawBorder = (columnWidths, config) => {
    const { separator, drawVerticalLine } = config;
    const columns = columnWidths.map((size) => {
        return config.separator.body.repeat(size);
    });
    return drawContent_1.drawContent(columns, {
        drawSeparator: drawVerticalLine,
        separatorGetter: (index, columnCount) => {
            if (index === 0) {
                return separator.left;
            }
            if (index === columnCount) {
                return separator.right;
            }
            return separator.join;
        },
    }) + '\n';
};
exports.drawBorder = drawBorder;
const drawBorderTop = (columnWidths, config) => {
    const result = drawBorder(columnWidths, {
        ...config,
        separator: {
            body: config.border.topBody,
            join: config.border.topJoin,
            left: config.border.topLeft,
            right: config.border.topRight,
        },
    });
    if (result === '\n') {
        return '';
    }
    return result;
};
exports.drawBorderTop = drawBorderTop;
const drawBorderJoin = (columnWidths, config) => {
    return drawBorder(columnWidths, {
        ...config,
        separator: {
            body: config.border.joinBody,
            join: config.border.joinJoin,
            left: config.border.joinLeft,
            right: config.border.joinRight,
        },
    });
};
exports.drawBorderJoin = drawBorderJoin;
const drawBorderBottom = (columnWidths, config) => {
    return drawBorder(columnWidths, {
        ...config,
        separator: {
            body: config.border.bottomBody,
            join: config.border.bottomJoin,
            left: config.border.bottomLeft,
            right: config.border.bottomRight,
        },
    });
};
exports.drawBorderBottom = drawBorderBottom;
const createTableBorderGetter = (columnWidths, config) => {
    return (index, size) => {
        if (!config.header) {
            if (index === 0) {
                return drawBorderTop(columnWidths, config);
            }
            if (index === size) {
                return drawBorderBottom(columnWidths, config);
            }
            return drawBorderJoin(columnWidths, config);
        }
        // Deal with the header
        if (index === 0) {
            return drawBorderTop(columnWidths, {
                ...config,
                border: {
                    ...config.border,
                    topJoin: config.border.topBody,
                },
            });
        }
        if (index === 1) {
            return drawBorderJoin(columnWidths, {
                ...config,
                border: {
                    ...config.border,
                    joinJoin: config.border.headerJoin,
                },
            });
        }
        if (index === size) {
            return drawBorderBottom(columnWidths, config);
        }
        return drawBorderJoin(columnWidths, config);
    };
};
exports.createTableBorderGetter = createTableBorderGetter;
