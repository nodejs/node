"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.distributeUnevenly = exports.countSpaceSequence = exports.groupBySizes = exports.makeBorderConfig = exports.splitAnsi = exports.normalizeString = void 0;
const slice_ansi_1 = __importDefault(require("slice-ansi"));
const strip_ansi_1 = __importDefault(require("strip-ansi"));
const getBorderCharacters_1 = require("./getBorderCharacters");
/**
 * Converts Windows-style newline to Unix-style
 *
 * @internal
 */
const normalizeString = (input) => {
    return input.replace(/\r\n/g, '\n');
};
exports.normalizeString = normalizeString;
/**
 * Splits ansi string by newlines
 *
 * @internal
 */
const splitAnsi = (input) => {
    const lengths = strip_ansi_1.default(input).split('\n').map(({ length }) => {
        return length;
    });
    const result = [];
    let startIndex = 0;
    lengths.forEach((length) => {
        result.push(length === 0 ? '' : slice_ansi_1.default(input, startIndex, startIndex + length));
        // Plus 1 for the newline character itself
        startIndex += length + 1;
    });
    return result;
};
exports.splitAnsi = splitAnsi;
/**
 * Merges user provided border characters with the default border ("honeywell") characters.
 *
 * @internal
 */
const makeBorderConfig = (border) => {
    return {
        ...getBorderCharacters_1.getBorderCharacters('honeywell'),
        ...border,
    };
};
exports.makeBorderConfig = makeBorderConfig;
/**
 * Groups the array into sub-arrays by sizes.
 *
 * @internal
 * @example
 * groupBySizes(['a', 'b', 'c', 'd', 'e'], [2, 1, 2]) = [ ['a', 'b'], ['c'], ['d', 'e'] ]
 */
const groupBySizes = (array, sizes) => {
    let startIndex = 0;
    return sizes.map((size) => {
        const group = array.slice(startIndex, startIndex + size);
        startIndex += size;
        return group;
    });
};
exports.groupBySizes = groupBySizes;
/**
 * Counts the number of continuous spaces in a string
 *
 * @internal
 * @example
 * countGroupSpaces('a  bc  de f') = 3
 */
const countSpaceSequence = (input) => {
    var _a, _b;
    return (_b = (_a = input.match(/\s+/g)) === null || _a === void 0 ? void 0 : _a.length) !== null && _b !== void 0 ? _b : 0;
};
exports.countSpaceSequence = countSpaceSequence;
/**
 * Creates the non-increasing number array given sum and length
 * whose the difference between maximum and minimum is not greater than 1
 *
 * @internal
 * @example
 * distributeUnevenly(6, 3) = [2, 2, 2]
 * distributeUnevenly(8, 3) = [3, 3, 2]
 */
const distributeUnevenly = (sum, length) => {
    const result = Array.from({ length }).fill(Math.floor(sum / length));
    return result.map((element, index) => {
        return element + (index < sum % length ? 1 : 0);
    });
};
exports.distributeUnevenly = distributeUnevenly;
