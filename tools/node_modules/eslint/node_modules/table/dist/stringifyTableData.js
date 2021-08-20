"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.stringifyTableData = void 0;
const utils_1 = require("./utils");
const stringifyTableData = (rows) => {
    return rows.map((cells) => {
        return cells.map((cell) => {
            return utils_1.normalizeString(String(cell));
        });
    });
};
exports.stringifyTableData = stringifyTableData;
