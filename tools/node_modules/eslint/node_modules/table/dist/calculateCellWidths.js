"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.calculateCellWidths = void 0;
const string_width_1 = __importDefault(require("string-width"));
/**
 * Calculates width of each cell contents in a row.
 */
const calculateCellWidths = (cells) => {
    return cells.map((cell) => {
        return Math.max(...cell.split('\n').map(string_width_1.default));
    });
};
exports.calculateCellWidths = calculateCellWidths;
