"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.truncateTableData = exports.truncateString = void 0;
const lodash_truncate_1 = __importDefault(require("lodash.truncate"));
const truncateString = (input, length) => {
    return lodash_truncate_1.default(input, { length,
        omission: '…' });
};
exports.truncateString = truncateString;
/**
 * @todo Make it work with ASCII content.
 */
const truncateTableData = (rows, config) => {
    return rows.map((cells) => {
        return cells.map((cell, cellIndex) => {
            return exports.truncateString(cell, config.columns[cellIndex].truncate);
        });
    });
};
exports.truncateTableData = truncateTableData;
