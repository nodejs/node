"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.createStream = void 0;
const alignTableData_1 = require("./alignTableData");
const calculateRowHeights_1 = require("./calculateRowHeights");
const drawBorder_1 = require("./drawBorder");
const drawRow_1 = require("./drawRow");
const makeStreamConfig_1 = require("./makeStreamConfig");
const mapDataUsingRowHeights_1 = require("./mapDataUsingRowHeights");
const padTableData_1 = require("./padTableData");
const stringifyTableData_1 = require("./stringifyTableData");
const truncateTableData_1 = require("./truncateTableData");
const prepareData = (data, config) => {
    let rows = stringifyTableData_1.stringifyTableData(data);
    rows = truncateTableData_1.truncateTableData(rows, config);
    const rowHeights = calculateRowHeights_1.calculateRowHeights(rows, config);
    rows = mapDataUsingRowHeights_1.mapDataUsingRowHeights(rows, rowHeights, config);
    rows = alignTableData_1.alignTableData(rows, config);
    rows = padTableData_1.padTableData(rows, config);
    return rows;
};
const create = (row, columnWidths, config) => {
    const rows = prepareData([row], config);
    const body = rows.map((literalRow) => {
        return drawRow_1.drawRow(literalRow, config);
    }).join('');
    let output;
    output = '';
    output += drawBorder_1.drawBorderTop(columnWidths, config);
    output += body;
    output += drawBorder_1.drawBorderBottom(columnWidths, config);
    output = output.trimEnd();
    process.stdout.write(output);
};
const append = (row, columnWidths, config) => {
    const rows = prepareData([row], config);
    const body = rows.map((literalRow) => {
        return drawRow_1.drawRow(literalRow, config);
    }).join('');
    let output = '';
    const bottom = drawBorder_1.drawBorderBottom(columnWidths, config);
    if (bottom !== '\n') {
        output = '\r\u001B[K';
    }
    output += drawBorder_1.drawBorderJoin(columnWidths, config);
    output += body;
    output += bottom;
    output = output.trimEnd();
    process.stdout.write(output);
};
const createStream = (userConfig) => {
    const config = makeStreamConfig_1.makeStreamConfig(userConfig);
    const columnWidths = Object.values(config.columns).map((column) => {
        return column.width + column.paddingLeft + column.paddingRight;
    });
    let empty = true;
    return {
        write: (row) => {
            if (row.length !== config.columnCount) {
                throw new Error('Row cell count does not match the config.columnCount.');
            }
            if (empty) {
                empty = false;
                create(row, columnWidths, config);
            }
            else {
                append(row, columnWidths, config);
            }
        },
    };
};
exports.createStream = createStream;
