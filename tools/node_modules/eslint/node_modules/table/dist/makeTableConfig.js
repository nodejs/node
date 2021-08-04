"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.makeTableConfig = void 0;
const lodash_clonedeep_1 = __importDefault(require("lodash.clonedeep"));
const calculateColumnWidths_1 = __importDefault(require("./calculateColumnWidths"));
const utils_1 = require("./utils");
const validateConfig_1 = require("./validateConfig");
/**
 * Creates a configuration for every column using default
 * values for the missing configuration properties.
 */
const makeColumnsConfig = (rows, columns, columnDefault) => {
    const columnWidths = calculateColumnWidths_1.default(rows);
    return rows[0].map((_, columnIndex) => {
        return {
            alignment: 'left',
            paddingLeft: 1,
            paddingRight: 1,
            truncate: Number.POSITIVE_INFINITY,
            verticalAlignment: 'top',
            width: columnWidths[columnIndex],
            wrapWord: false,
            ...columnDefault,
            ...columns === null || columns === void 0 ? void 0 : columns[columnIndex],
        };
    });
};
const makeHeaderConfig = (config) => {
    if (!config.header) {
        return undefined;
    }
    return {
        alignment: 'center',
        paddingLeft: 1,
        paddingRight: 1,
        truncate: Number.POSITIVE_INFINITY,
        wrapWord: false,
        ...config.header,
    };
};
/**
 * Makes a new configuration object out of the userConfig object
 * using default values for the missing configuration properties.
 */
const makeTableConfig = (rows, userConfig = {}) => {
    var _a, _b, _c;
    validateConfig_1.validateConfig('config.json', userConfig);
    const config = lodash_clonedeep_1.default(userConfig);
    return {
        ...config,
        border: utils_1.makeBorderConfig(config.border),
        columns: makeColumnsConfig(rows, config.columns, config.columnDefault),
        drawHorizontalLine: (_a = config.drawHorizontalLine) !== null && _a !== void 0 ? _a : (() => {
            return true;
        }),
        drawVerticalLine: (_b = config.drawVerticalLine) !== null && _b !== void 0 ? _b : (() => {
            return true;
        }),
        header: makeHeaderConfig(config),
        singleLine: (_c = config.singleLine) !== null && _c !== void 0 ? _c : false,
    };
};
exports.makeTableConfig = makeTableConfig;
